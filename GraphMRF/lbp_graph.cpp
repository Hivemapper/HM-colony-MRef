/*
 * Copyright (C) 2015, Nils Moehrle, Benjamin Richter
 * TU Darmstadt - Graphics, Capture and Massively Parallel Computing
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD 3-Clause license. See the LICENSE.txt file for details.
 */

#include <algorithm>
#include "lbp_graph.h"
#include <iostream>

MRF_NAMESPACE_BEGIN

  LBPGraph::LBPGraph(int num_sites, int) :
      vertices(num_sites) {}

  ENERGY_TYPE LBPGraph::compute_energy() {
    ENERGY_TYPE energy = 0;
    ENERGY_TYPE energydc = 0;
    ENERGY_TYPE energyscf = 0;

#pragma omp parallel for reduction(+:energydc)
    for (std::size_t vertex_idx = 0; vertex_idx < vertices.size(); ++vertex_idx) {
      Vertex const &vertex = vertices[vertex_idx];
      energydc += vertex.data_cost;
    }

#pragma omp parallel for reduction(+:energyscf)
    for (std::size_t edge_idx = 0; edge_idx < edges.size(); ++edge_idx) {
      DirectedEdge const &edge = edges[edge_idx];
      energyscf += smooth_cost_func(edge.v1, edge.v2,
                                 vertices[edge.v1].label, vertices[edge.v2].label);
    }

    std::cout <<"energy_cost_data: "<< energydc << "\t energy_smooth_cf: "<< energyscf << std::endl;
    energy = energydc + energyscf;
    return energy;
  }

// Optimization Function
  ENERGY_TYPE LBPGraph::optimize(int num_iterations) {

    for (int i = 0; i < num_iterations; ++i) {

#pragma omp parallel for
      for (std::size_t edge_idx = 0; edge_idx < edges.size(); ++edge_idx) {
        DirectedEdge &edge = edges[edge_idx];

        // For each edge get verts v1,v2 and their labels
        std::vector<int> const &labels1 = vertices[edge.v1].labels;
        std::vector<int> const &labels2 = vertices[edge.v2].labels;

        // Skim content of label string 2
        for (std::size_t j = 0; j < labels2.size(); ++j) {
          int label2 = labels2[j];
          ENERGY_TYPE min_energy = std::numeric_limits<ENERGY_TYPE>::max();

          // Skim content of label string 1
          for (std::size_t k = 0; k < labels1.size(); ++k) {
            int label1 = labels1[k];

            // For each pair l1 <-> l2 a cost is composed ( data + smoothness )
            ENERGY_TYPE energy = smooth_cost_func(edge.v1, edge.v2, label1, label2)
                                 + vertices[edge.v1].data_costs[k];

            std::vector<int> const &incoming_edges1 = vertices[edge.v1].incoming_edges;

            // Skim over all incomming edges of vertex
            for (std::size_t n = 0; n < incoming_edges1.size(); ++n) {
              DirectedEdge const &pre_edge = edges[incoming_edges1[n]];
              if (pre_edge.v1 == edge.v2) continue;
              energy += pre_edge.old_msg[k];
            }

            // Save current min energy in label string 1
            if (energy < min_energy)
              min_energy = energy;
          }
          edge.new_msg[j] = min_energy;
        }
      }

#pragma omp parallel for
      for (std::size_t edge_idx = 0; edge_idx < edges.size(); ++edge_idx) {
        DirectedEdge &edge = edges[edge_idx];
        edge.new_msg.swap(edge.old_msg);
        ENERGY_TYPE min_msg = std::numeric_limits<ENERGY_TYPE>::max();
        for (ENERGY_TYPE msg : edge.old_msg)
          min_msg = std::min(min_msg, msg);
        for (ENERGY_TYPE &msg : edge.old_msg)
          msg -= min_msg;
      }
    }

#pragma omp parallel for
    for (std::size_t vertex_idx = 0; vertex_idx < vertices.size(); ++vertex_idx) {
      Vertex &vertex = vertices[vertex_idx];
      ENERGY_TYPE min_energy = std::numeric_limits<ENERGY_TYPE>::max();
      for (std::size_t j = 0; j < vertex.labels.size(); ++j) {
        ENERGY_TYPE energy = vertex.data_costs[j];
        for (int incoming_edge_idx : vertex.incoming_edges) {
          energy += edges[incoming_edge_idx].old_msg[j];
        }
        if (energy < min_energy) {
          min_energy = energy;
          vertex.label = vertex.labels[j];
          vertex.data_cost = vertex.data_costs[j];
        }
      }
    }

    return compute_energy();
  }

  void LBPGraph::set_smooth_cost(SmoothCostFunction func) {
    smooth_cost_func = func;
  }

  void LBPGraph::set_neighbors(int site1, int site2) {
    edges.push_back(DirectedEdge(site1, site2));
    vertices[site2].incoming_edges.push_back(edges.size() - 1);
    edges.push_back(DirectedEdge(site2, site1));
    vertices[site1].incoming_edges.push_back(edges.size() - 1);
  }

  void LBPGraph::set_neighbors_single(int site1, int site2) {
    edges.push_back(DirectedEdge(site1, site2));
    vertices[site2].incoming_edges.push_back(edges.size() - 1);
  }

// So we are pushing this in layers of a constant label
// costs: c. site, c.cost
  void LBPGraph::set_data_costs(int label, std::vector <SparseDataCost> const &costs) {

    // Skim the cost vector lenght are vertices...
    for (std::size_t i = 0; i < costs.size(); ++i) {

      Vertex &vertex = vertices[costs[i].site];
      vertex.labels.push_back(label);
      ENERGY_TYPE data_cost = costs[i].cost;
      vertex.data_costs.push_back(data_cost);
      // Here's where the min-cost label is chosen. As it stands, there's
      //   no cost to change a label value, so it could just pick
      //   the first of several 0's if the point is not penalized in any
      //   class (all costs = 0)
// This stores the minimum cost and label guess some init
      if (data_cost < vertex.data_cost) {
        vertex.label = label;
        vertex.data_cost = data_cost;
      }
// This inits the message per label
      for (int j : vertex.incoming_edges) {
        DirectedEdge &incoming_edge = edges[j];
        incoming_edge.old_msg.push_back(0);
        incoming_edge.new_msg.push_back(0);
      }
    }
  }

  int LBPGraph::what_label(int site) {
    return vertices[site].label;
  }

  int LBPGraph::num_sites() {
    return static_cast<int>(vertices.size());
  }

MRF_NAMESPACE_END
