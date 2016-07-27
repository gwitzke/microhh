/*
 * MicroHH
 * Copyright (c) 2011-2015 Chiel van Heerwaarden
 * Copyright (c) 2011-2015 Thijs Heus
 * Copyright (c) 2014-2015 Bart van Stratum
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

#ifndef IMMERSED_BOUNDARY
#define IMMERSED_BOUNDARY

#include <vector>
#include <bitset>

class Model;
class Grid;
class Fields;
class Input;
class Stats;
struct Mask;

struct Neighbour
{
    int i;
    int j;
    int k;
    double distance;
};

struct Ghost_cell
{
    int i;  ///< i-location of ghost cell
    int j;  ///< j-location of ghost cell
    int k;  ///< k-location of ghost cell

    double xb;  ///< Nearest location at boundary
    double yb;  ///< Nearest location at boundary
    double zb;  ///< Nearest location at boundary

    std::vector< std::vector<double> > B;    ///< Inversed matrix with the locations of all interpolation points
    std::vector<Neighbour> neighbours; ///< Neighbouring fluid points used in interpolation
};

class Immersed_boundary
{
    public:
        enum IB_type{None_type, Sine_type, Gaus_type, Block_type, User_type};

        Immersed_boundary(Model*, Input*); ///< Constructor of the class.
        ~Immersed_boundary();              ///< Destructor of the class.

        void init();
        void create();
        void exec();
        void exec_tend();

        void exec_stats(Mask*); ///< Execute statistics of immersed boundaries

    private:
        Model*  model;  ///< Pointer to model class.
        Fields* fields; ///< Pointer to fields class.
        Grid*   grid;   ///< Pointer to grid class.
        Stats*  stats;  ///< Pointer to grid class.

        std::vector<Ghost_cell> ghost_cells_u;  ///< Vector holding info on all the ghost cells within the boundary
        std::vector<Ghost_cell> ghost_cells_v;  ///< Vector holding info on all the ghost cells within the boundary
        std::vector<Ghost_cell> ghost_cells_w;  ///< Vector holding info on all the ghost cells within the boundary
        std::vector<Ghost_cell> ghost_cells_s;  ///< Vector holding info on all the ghost cells within the boundary

        template<IB_type, int> 
        void find_ghost_cells(std::vector<Ghost_cell>*, const double*, const double*, const double*); ///< Function which determines the ghost cells

        template<IB_type, int> 
        double boundary_function(double, double); ///< Function describing boundary (1D)
        template<IB_type, int> 
        bool is_ghost_cell(const double*, const double*, const double*, const int, const int, const int); ///< Function which checks if a cell is a ghost cell
        template<IB_type, int> 
        void find_nearest_location_wall(double&, double&, double&, double&, 
                                        const double, const double, const double, const int, const int, const int); ///< Function which checks if a cell is a ghost cell
        template<IB_type, int> 
        void find_interpolation_points(Ghost_cell&, const double*, const double*, const double*, const int, const int, const int, const int); ///< Function which checks if a cell is a ghost cell

        void define_distance_matrix(Ghost_cell&, const double*, const double*, const double*);

        // General settings IB
        std::string sw_ib; ///< Immersed boundary switch
        IB_type ib_type;   ///< Internal IB switch

        double amplitude;  ///< Height of IB object (Gaussian, sine or blocks)
        double z_offset;   ///< Vertical offset of IB objects
        int xy_dims;       ///< Hill dimension (1=x, 2=xy)

        // Sine type of boundary
        double wavelength_x; ///< Wave length sine in x-direction
        double wavelength_y; ///< Wave length sine in y-direction

        // Gaussian hill
        double x0_hill;
        double y0_hill;
        double sigma_x_hill;
        double sigma_y_hill;
};
#endif
