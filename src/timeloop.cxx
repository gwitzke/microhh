/*
 * MicroHH
 * Copyright (c) 2011-2017 Chiel van Heerwaarden
 * Copyright (c) 2011-2017 Thijs Heus
 * Copyright (c) 2014-2017 Bart van Stratum
 *
 * This file is part of MicroHH
 *
 * MicroHH is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * MicroHH is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with MicroHH.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdio>
#include <cmath>
#include "input.h"
#include "master.h"
#include "grid.h"
#include "fields.h"
#include "timeloop.h"
#include "defines.h"
#include "constants.h"
#include "model.h"

Timeloop::Timeloop(Model* modelin, Input* inputin)
{
    model  = modelin;
    grid   = model->grid;
    fields = model->fields;
    master = model->master;

    substep = 0;
    ifactor = 1e9;

    // input parameters
    int n = 0;

    // obligatory parameters
    if (master->mode == "init")
        starttime = 0.;
    else
        n += inputin->get_item(&starttime, "time", "starttime", "");

    n += inputin->get_item(&endtime , "time", "endtime" , "");
    n += inputin->get_item(&savetime, "time", "savetime", "");

    // optional parameters
    n += inputin->get_item(&adaptivestep, "time", "adaptivestep", "", true            );
    n += inputin->get_item(&dtmax       , "time", "dtmax"       , "", Constants::dbig );
    n += inputin->get_item(&dt          , "time", "dt"          , "", dtmax           );
    n += inputin->get_item(&rkorder     , "time", "rkorder"     , "", 3               );
    n += inputin->get_item(&outputiter  , "time", "outputiter"  , "", 20              );
    n += inputin->get_item(&iotimeprec  , "time", "iotimeprec"  , "", 0               );

    if (master->mode == "post")
        n += inputin->get_item(&postproctime, "time", "postproctime", "");

    // if one argument fails, then crash
    if (n > 0)
        throw 1;

    // 3 and 4 are the only valid values for the rkorder
    if (!(rkorder == 3 || rkorder == 4))
    {
        master->print_error("\"%d\" is an illegal value for rkorder\n", rkorder);
        throw 1;
    }

    // initializations
    loop      = true;
    time      = 0.;
    iteration = 0;

    // set or calculate all the integer times
    itime         = (unsigned long) 0;

    // add 0.5 to prevent roundoff errors
    iendtime      = (unsigned long)(ifactor * endtime + 0.5);
    istarttime    = (unsigned long)(ifactor * starttime + 0.5);
    idt           = (unsigned long)(ifactor * dt + 0.5);
    idtmax        = (unsigned long)(ifactor * dtmax + 0.5);
    isavetime     = (unsigned long)(ifactor * savetime + 0.5);
    if (master->mode == "post")
        ipostproctime = (unsigned long)(ifactor * postproctime + 0.5);

    idtlim = idt;

    // take the proper precision for the output files into account
    iiotimeprec = (unsigned long)(ifactor * std::pow(10., iotimeprec) + 0.5);

    // check whether starttime and savetime are an exact multiple of iotimeprec
    if ((istarttime % iiotimeprec) || (isavetime % iiotimeprec))
    {
        master->print_error("starttime or savetime is not an exact multiple of iotimeprec\n");
        throw 1;
    }

    iotime = (int)(istarttime / iiotimeprec);

    gettimeofday(&start, NULL);

    if (master->mode == "init")
        inputin->flag_as_used("time", "starttime");
}

Timeloop::~Timeloop()
{
}

void Timeloop::set_time_step_limit()
{
    idtlim = idtmax;

    // Check whether the run should be stopped because of the wall clock limit
    if (master->at_wall_clock_limit())
    {
        // Set the time step to the nearest multiple of iotimeprec
        idtlim = std::min(idtlim, iiotimeprec - itime % iiotimeprec);
    }

    idtlim = std::min(idtlim, isavetime - itime % isavetime);
}

void Timeloop::set_time_step_limit(unsigned long idtlimin)
{
    idtlim = std::min(idtlim, idtlimin);
}

void Timeloop::step_time()
{
    // Only step forward in time if we are not in a substep
    if (in_substep())
        return;

    time  += dt;
    itime += idt;
    iotime = (int)(itime/iiotimeprec);

    ++iteration;

    if (itime >= iendtime)
        loop = false;
}

bool Timeloop::do_check()
{
    if (iteration % outputiter == 0 && !in_substep())
        return true;

    return false;
}

bool Timeloop::do_save()
{
    // Check whether the simulation has to stop due to the wallclock limit,
    // but only at a time step where actual saves can be made.
    if (itime % iiotimeprec == 0 && !in_substep() && master->at_wall_clock_limit())
    {
        master->print_warning("Simulation will be stopped after saving the restart files due to wall clock limit\n");

        // Stop looping
        loop = false;
        return true;
    }

    // Do not save directly after the start of the simulation and not in a substep
    if (itime % isavetime == 0 && iteration != 0 && !in_substep())
        return true;

    return false;
}

bool Timeloop::is_finished()
{
    // Return true if loop is false and vice versa.
    return !loop;
}

double Timeloop::check()
{
    gettimeofday(&end, NULL);

    double timeelapsed = (double)(end.tv_sec-start.tv_sec) + (double)(end.tv_usec-start.tv_usec) * 1.e-6;
    start = end;

    return timeelapsed;
}

void Timeloop::set_time_step()
{
    // Only set the time step if we are not in a substep
    if (in_substep())
        return;

    if (adaptivestep)
    {
        if (idt == 0)
        {
            master->print_error("Required time step less than precision %E of the time stepping\n", 1./ifactor);
            throw 1;
        }
        idt = idtlim;
        dt  = (double)idt / ifactor;
    }
}

void Timeloop::get_interpolation_factors(int& index0, int&index1, double& fac0, double& fac1, std::vector<double> timevec)
{
    // 1. Get the indexes and factors for the interpolation in time
    index0 = 0;
    index1 = 0;

    for (auto& t : timevec)
    {
        if (time < t)
            break;
        else
            ++index1;
    }

    // 2. Calculate the weighting factor, accounting for out of range situations where the simulation is longer than the time range in input
    if (index1 == 0)
    {
        fac0   = 0.;
        fac1   = 1.;
        index0 = 0;
    }
    else if (index1 == timevec.size())
    {
        fac0   = 1.;
        fac1   = 0.;
        index0 = index1-1;
        index1 = index0;
    }
    else
    {
        index0 = index1-1;
        fac0 = (timevec[index1] - time) / (timevec[index1] - timevec[index0]);
        fac1 = (time - timevec[index0]) / (timevec[index1] - timevec[index0]);
    }
}

#ifndef USECUDA
void Timeloop::exec()
{
    if (rkorder == 3)
    {
        for (FieldMap::const_iterator it = fields->at.begin(); it!=fields->at.end(); ++it)
            rk3(fields->ap[it->first]->data, it->second->data, dt);

        substep = (substep+1) % 3;
    }

    if (rkorder == 4)
    {
        for (FieldMap::const_iterator it = fields->at.begin(); it!=fields->at.end(); ++it)
            rk4(fields->ap[it->first]->data, it->second->data, dt);

        substep = (substep+1) % 5;
    }
}
#endif

double Timeloop::get_sub_time_step()
{
    double subdt = 0.;
    if (rkorder == 3)
        subdt = rk3subdt(dt);
    else if (rkorder == 4)
        subdt = rk4subdt(dt);

    return subdt;
}

inline double Timeloop::rk3subdt(const double dt)
{
    const double cB [] = {1./3., 15./16., 8./15.};
    return cB[substep]*dt;
}

inline double Timeloop::rk4subdt(const double dt)
{
    const double cB [] = {
        1432997174477./ 9575080441755.,
        5161836677717./13612068292357.,
        1720146321549./ 2090206949498.,
        3134564353537./ 4481467310338.,
        2277821191437./14882151754819.};
    return cB[substep]*dt;
}

void Timeloop::rk3(double * restrict a, double * restrict at, const double dt)
{
    const double cA [] = {0., -5./9., -153./128.};
    const double cB [] = {1./3., 15./16., 8./15.};

    const int jj = grid->icells;
    const int kk = grid->ijcells;

    for (int k=grid->kstart; k<grid->kend; k++)
        for (int j=grid->jstart; j<grid->jend; j++)
#pragma ivdep
            for (int i=grid->istart; i<grid->iend; i++)
            {
                const int ijk = i + j*jj + k*kk;
                a[ijk] += cB[substep]*dt*at[ijk];
            }

    const int substepn = (substep+1) % 3;

    // substep 0 resets the tendencies, because cA[0] == 0
    for (int k=grid->kstart; k<grid->kend; k++)
        for (int j=grid->jstart; j<grid->jend; j++)
#pragma ivdep
            for (int i=grid->istart; i<grid->iend; i++)
            {
                const int ijk = i + j*jj + k*kk;
                at[ijk] *= cA[substepn];
            }
}

void Timeloop::rk4(double * restrict a, double * restrict at, const double dt)
{
    const double cA [] = {
        0.,
        - 567301805773./1357537059087.,
        -2404267990393./2016746695238.,
        -3550918686646./2091501179385.,
        -1275806237668./ 842570457699.};

    const double cB [] = {
        1432997174477./ 9575080441755.,
        5161836677717./13612068292357.,
        1720146321549./ 2090206949498.,
        3134564353537./ 4481467310338.,
        2277821191437./14882151754819.};

    const int jj = grid->icells;
    const int kk = grid->ijcells;

    for (int k=grid->kstart; k<grid->kend; k++)
        for (int j=grid->jstart; j<grid->jend; j++)
#pragma ivdep
            for (int i=grid->istart; i<grid->iend; i++)
            {
                const int ijk = i + j*jj + k*kk;
                a[ijk] = a[ijk] + cB[substep]*dt*at[ijk];
            }

    const int substepn = (substep+1) % 5;

    // substep 0 resets the tendencies, because cA[0] == 0
    for (int k=grid->kstart; k<grid->kend; k++)
        for (int j=grid->jstart; j<grid->jend; j++)
#pragma ivdep
            for (int i=grid->istart; i<grid->iend; i++)
            {
                const int ijk = i + j*jj + k*kk;
                at[ijk] = cA[substepn]*at[ijk];
            }
}

bool Timeloop::in_substep()
{
    if (substep > 0)
        return true;
    else
        return false;
}

bool Timeloop::is_stats_step()
{
    // In case we are not in a substep and not at the first iteration
    // after a restart, we can could do statistics.
    if (!in_substep() && !((iteration > 0) && (itime == istarttime)))
        return true;
    else
        return false;
}

void Timeloop::save(int starttime)
{
    int nerror = 0;

    if (master->mpiid == 0)
    {
        char filename[256];
        std::sprintf(filename, "time.%07d", starttime);

        master->print_message("Saving \"%s\" ... ", filename);

        FILE *pFile;
        pFile = fopen(filename, "wbx");

        if (pFile == NULL)
        {
            master->print_message("FAILED\n", filename);
            ++nerror;
        }
        else
        {
            fwrite(&itime    , sizeof(unsigned long), 1, pFile);
            fwrite(&idt      , sizeof(unsigned long), 1, pFile);
            fwrite(&iteration, sizeof(int), 1, pFile);

            fclose(pFile);
            master->print_message("OK\n");
        }
    }

    // Broadcast the error code to prevent deadlocks in case of error.
    master->broadcast(&nerror, 1);
    if (nerror)
        throw 1;
}

void Timeloop::load(int starttime)
{
    int nerror = 0;

    if (master->mpiid == 0)
    {
        char filename[256];
        std::sprintf(filename, "time.%07d", starttime);

        master->print_message("Loading \"%s\" ... ", filename);

        FILE *pFile;
        pFile = fopen(filename, "rb");

        if (pFile == NULL)
        {
            master->print_error("\"%s\" does not exist\n", filename);
            ++nerror;
        }
        else
        {
            fread(&itime    , sizeof(unsigned long), 1, pFile);
            fread(&idt      , sizeof(unsigned long), 1, pFile);
            fread(&iteration, sizeof(int), 1, pFile);

            fclose(pFile);
        }
        master->print_message("OK\n");
    }

    master->broadcast(&nerror, 1);
    if (nerror)
        throw 1;

    master->broadcast(&itime    , 1);
    master->broadcast(&idt      , 1);
    master->broadcast(&iteration, 1);

    // calculate the double precision time from the integer time
    time = (double)itime / ifactor;
    dt   = (double)idt   / ifactor;
}

void Timeloop::step_post_proc_time()
{
    itime += ipostproctime;
    iotime = (int)(itime/iiotimeprec);

    if (itime > iendtime)
        loop = false;
}
