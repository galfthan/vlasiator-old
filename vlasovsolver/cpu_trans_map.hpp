/*
  This file is part of Vlasiator.
  Copyright 2014-2015 Finnish Meteorological Institute
*/
#ifndef CPU_TRANS_MAP_H
#define CPU_TRANS_MAP_H

#include <vector>

#include "vec.h"
#include "../common.h"
#include "../spatial_cell.hpp"

void clearTargetGrid(const std::vector<SpatialCell*>& cells);
void createTargetGrid(const std::vector<SpatialCell*>& cells, const int& popID);
bool do_translate_cell(spatial_cell::SpatialCell* SC);
void swapTargetSourceGrid(const std::vector<SpatialCell*>& cells, const int& popID);
bool trans_map_1d(const dccrg::Dccrg<spatial_cell::SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
		  const CellID cellID, SpatialCell** source_neighbors, SpatialCell** target_neighbors, 
		  const uint dimension,const Realv dt,const int& popID);
void update_remote_mapping_contribution(dccrg::Dccrg<spatial_cell::SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
        const uint dimension,int direction,const int& popID);
void zeroTargetGrid(const std::vector<SpatialCell*>& cells);

  
void compute_spatial_source_neighbors(const dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
                                      const CellID& cellID,
                                      const uint dimension,
                                      SpatialCell **neighbors);
void compute_spatial_target_neighbors(const dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
                                      const CellID& cellID,
                                      const uint dimension,
                                      SpatialCell **neighbors);



#endif
