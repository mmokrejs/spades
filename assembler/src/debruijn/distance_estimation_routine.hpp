//***************************************************************************
//* Copyright (c) 2011-2012 Saint-Petersburg Academic University
//* All Rights Reserved
//* See file LICENSE for details.
//****************************************************************************

/*
 * distance_estimation.hpp
 *
 *  Created on: 1 Sep 2011
 *      Author: valery
 */

#pragma once

#include "standard.hpp"
#include "dataset_readers.hpp"
#include "late_pair_info_count.hpp"
#include "gap_closer.hpp"
#include "check_tools.hpp"

#include "de/paired_info.hpp"
#include "de/weighted_distance_estimation.hpp"
#include "de/extensive_distance_estimation.hpp"
#include "de/smoothing_distance_estimation.hpp"

#include <set>

namespace debruijn_graph {

typedef set<Point> Histogram;

void estimate_with_estimator(const Graph& graph,
                             const AbstractDistanceEstimator<Graph>& estimator,
                             const PairedInfoNormalizer<Graph>& normalizer,
                             const PairInfoWeightFilter<Graph>& filter,
                             PairedIndexT& clustered_index) 
{
  INFO("Estimating distances");
  PairedIndexT raw_clustered_index(graph);

  if (cfg::get().use_multithreading) {
    estimator.EstimateParallel(raw_clustered_index, cfg::get().max_threads);
  } else {
    estimator.Estimate(raw_clustered_index);
  }

  INFO("Normalizing Weights");
  PairedIndexT normalized_index(graph);

    // temporary fix for scaffolding (I hope) due to absolute thresholds in path_extend
  if (cfg::get().est_mode == debruijn_graph::estimation_mode::em_weighted 
   || cfg::get().est_mode == debruijn_graph::estimation_mode::em_smoothing
   || cfg::get().est_mode == debruijn_graph::estimation_mode::em_extensive) 
  {
    //TODO: add to config
    double coeff = (cfg::get().ds.single_cell ? (10. / 80.) : (0.2 / 3.00) );
    normalizer.FillNormalizedIndex(raw_clustered_index, normalized_index, coeff);
  } 
  else
    normalizer.FillNormalizedIndex(raw_clustered_index, normalized_index);

  INFO("Filtering info");
  filter.Filter(normalized_index, clustered_index);
  DEBUG("Info Filtered");
}

void estimate_distance(conj_graph_pack& gp, 
                       const PairedIndexT& paired_index,
                             PairedIndexT& clustered_index) 
{
  if (!cfg::get().developer_mode) {
    clustered_index.Attach();
    clustered_index.Init();
  }

  if (cfg::get().paired_mode) {
    INFO("STAGE == Estimating Distance");

    double is_var = *cfg::get().ds.is_var;
    size_t delta = size_t(is_var);
    size_t linkage_distance = size_t(cfg::get().de.linkage_distance_coeff * is_var);
    GraphDistanceFinder<Graph> dist_finder(gp.g, *cfg::get().ds.IS, *cfg::get().ds.RL, delta);
    size_t max_distance = size_t(cfg::get().de.max_distance_coeff * is_var);
    boost::function<double(int)> weight_function;

    if (cfg::get().est_mode == debruijn_graph::estimation_mode::em_weighted 
     || cfg::get().est_mode == debruijn_graph::estimation_mode::em_smoothing
     || cfg::get().est_mode == debruijn_graph::estimation_mode::em_extensive)
    {
      INFO("Retaining insert size distribution for it");
      map<int, size_t> insert_size_hist = cfg::get().ds.hist;
      if (insert_size_hist.size() == 0) {
        auto streams = paired_binary_readers(false, 0);
        GetInsertSizeHistogram(streams, gp, *cfg::get().ds.IS, *cfg::get().ds.is_var, 
                               insert_size_hist);
      }
      WeightDEWrapper wrapper(insert_size_hist, *cfg::get().ds.IS);
      INFO("Weight Wrapper Done");
      weight_function = boost::bind(&WeightDEWrapper::CountWeight, wrapper, _1);
    } else
      weight_function = UnityFunction;

    PairedInfoNormalizer<Graph>::WeightNormalizer normalizing_f;
    if (cfg::get().ds.single_cell) {
      normalizing_f = &TrivialWeightNormalization<Graph>;
    } else {
      // todo reduce number of constructor params
      PairedInfoWeightNormalizer<Graph> weight_normalizer(gp.g,
          *cfg::get().ds.IS, *cfg::get().ds.is_var, *cfg::get().ds.RL,
          gp.k_value, *cfg::get().ds.avg_coverage);
      normalizing_f = boost::bind(
          &PairedInfoWeightNormalizer<Graph>::NormalizeWeight,
          weight_normalizer, _1, _2, _3);
    }
    PairedInfoNormalizer<Graph> normalizer(normalizing_f);
    INFO("Normalizer Done");

    PairInfoWeightFilter<Graph> filter(gp.g, cfg::get().de.filter_threshold);
    INFO("Weight Filter Done");

    switch (cfg::get().est_mode) 
    {
      case debruijn_graph::estimation_mode::em_simple : 
        {
          const AbstractDistanceEstimator<Graph>& 
              estimator =
                  DistanceEstimator<Graph>(gp.g, paired_index, dist_finder, 
                                              linkage_distance, max_distance);

          estimate_with_estimator(gp.g, estimator, normalizer, filter, clustered_index);
          break;
        }
      case debruijn_graph::estimation_mode::em_weighted :
        {
          const AbstractDistanceEstimator<Graph>& 
              estimator = 
                  WeightedDistanceEstimator<Graph>(gp.g, paired_index,
                      dist_finder, weight_function, linkage_distance, max_distance);

          estimate_with_estimator(gp.g, estimator, normalizer, filter, clustered_index);
          break;
        }
      case debruijn_graph::estimation_mode::em_extensive :
        {
          const AbstractDistanceEstimator<Graph>& 
              estimator =
                  ExtensiveDistanceEstimator<Graph>(gp.g, paired_index,
                      dist_finder, weight_function, linkage_distance, max_distance);

          estimate_with_estimator(gp.g, estimator, normalizer, filter, clustered_index);
          break;
        }
      case debruijn_graph::estimation_mode::em_smoothing :
        {
          const AbstractDistanceEstimator<Graph>& 
              estimator =
                  SmoothingDistanceEstimator<Graph>(gp.g, paired_index,
                      dist_finder, weight_function, linkage_distance, max_distance,
                      cfg::get().ade.threshold,
                      cfg::get().ade.range_coeff,
                      cfg::get().ade.delta_coeff, cfg::get().ade.cutoff,
                      cfg::get().ade.min_peak_points,
                      cfg::get().ade.inv_density,
                      cfg::get().ade.percentage,
                      cfg::get().ade.derivative_threshold);

          estimate_with_estimator(gp.g, estimator, normalizer, filter, clustered_index);
          break;
        }
    }

    INFO("Refining clustered pair information");
    RefinePairedInfo(gp.g, clustered_index);
    INFO("The refining of clustered pair information has been finished");

    //experimental [here we can remove some edges from the graph]
    if (cfg::get().simp.simpl_mode
        == debruijn_graph::simplification_mode::sm_pair_info_aware) {
      EdgeQuality<Graph> quality_handler(gp.g, gp.index, gp.kmer_mapper, gp.genome);

      QualityLoggingRemovalHandler<Graph> qual_removal_handler(gp.g, quality_handler);
      boost::function<void(EdgeId)> removal_handler_f = boost::bind(
          &QualityLoggingRemovalHandler<Graph>::HandleDelete,
          &qual_removal_handler, _1);
      EdgeRemover<Graph> edge_remover(gp.g, true, removal_handler_f);
      INFO("Pair info aware ErroneousConnectionsRemoval");
      RemoveEroneousEdgesUsingPairedInfo(gp.g, paired_index,
          edge_remover);
      INFO("Pair info aware ErroneousConnectionsRemoval stats");
      CountStats(gp.g, gp.index, gp.genome, gp.k_value);
    }
    //experimental

  }
}

void load_distance_estimation(conj_graph_pack& gp,
                              PairedIndexT& paired_index, 
                              PairedIndexT& clustered_index,
                              path::files_t* used_files)
{
  string p = path::append_path(cfg::get().load_from, "distance_estimation");
  used_files->push_back(p);
  ScanAll(p, gp, paired_index, clustered_index);
  load_estimated_params(p);
}

void save_distance_estimation(const conj_graph_pack& gp, 
                              const PairedIndexT& paired_index, 
                              const PairedIndexT& clustered_index)
{
  if (cfg::get().make_saves) {
    string p = path::append_path(cfg::get().output_saves, "distance_estimation");
    PrintAll(p, gp, paired_index, clustered_index);
    write_estimated_params(p);
  }
}

void count_estimated_info_stats(const conj_graph_pack& gp,
                                const PairedIndexT& paired_index, 
                                const PairedIndexT& clustered_index)
{
  CountClusteredPairedInfoStats(gp, paired_index, clustered_index);
}

void exec_distance_estimation(conj_graph_pack& gp,
                             PairedIndexT& paired_index, 
                             PairedIndexT& clustered_index)
{
  if (cfg::get().entry_point <= ws_distance_estimation) {
    exec_late_pair_info_count(gp, paired_index);
    estimate_distance(gp, paired_index, clustered_index);
    save_distance_estimation(gp, paired_index, clustered_index);
    if (cfg::get().paired_mode && cfg::get().paired_info_statistics)
      count_estimated_info_stats(gp, paired_index, clustered_index);
  } else {
    INFO("Loading Distance Estimation");
    path::files_t used_files;
    load_distance_estimation(gp, paired_index, clustered_index, &used_files);
    link_files_by_prefix(used_files, cfg::get().output_saves);
  }
}

}
