/*
 * GHOST (General meta-Heuristic Optimization Solving Tool) is a C++ framework
 * designed to help developers to model and implement optimization problem
 * solving. It contains a meta-heuristic solver aiming to solve any kind of
 * combinatorial and optimization real-time problems represented by a CSP/COP/EF-CSP/EF-COP. 
 *
 * First developed to solve game-related optimization problems, GHOST can be used for
 * any kind of applications where solving combinatorial and optimization problems. In
 * particular, it had been designed to be able to solve not-too-complex problem instances
 * within some milliseconds, making it very suitable for highly reactive or embedded systems.
 * Please visit https://github.com/richoux/GHOST for further information.
 *
 * Copyright (C) 2014-2026 Florian Richoux
 *
 * This file is part of GHOST.
 * GHOST is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * GHOST is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with GHOST. If not, see http://www.gnu.org/licenses/.
 */

#include <limits>
#include <numeric>

#include "variable.hpp"

using ghost::Variable;

Variable::Variable( int value,
                    std::vector<int>* domain,
                    const std::string& name )
	: _id( _count++ ),
	  _name( name ),
	  _current_value( value ),
	  _domain( domain )
{ }

// Variable::Variable( int starting_value, std::size_t size, int index, const std::string& name )
// 	: _domain( std::vector<int>( size ) ),
// 	  _id( 0 ),
// 	  _name( name ),
// 	  _min_value( starting_value ),
// 	  _max_value( starting_value + static_cast<int>( size ) - 1 )
// {
// 	std::iota( _domain.begin(), _domain.end(), starting_value );
// 	_current_value = _domain.at( index );
// }

// Variable::Variable( int starting_value,
//                     std::size_t size,
//                     const std::string& name )
// 	: Variable( starting_value, size, 0, name )
// { }
