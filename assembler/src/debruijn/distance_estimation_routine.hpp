/*
 * distance_estimation.hpp
 *
 *  Created on: 1 Sep 2011
 *      Author: valery
 */

#pragma once

#include "standard.hpp"
#include "omni/paired_info.hpp"
#include "late_pair_info_count.hpp"

#include "check_tools.hpp"

namespace debruijn_graph {

void estimate_distance(conj_graph_pack& gp,
		paired_info_index& paired_index, paired_info_index& clustered_index);

} // debruijn_graph

// move impl to *.cpp

namespace debruijn_graph {

void estimate_distance(conj_graph_pack& gp,
		paired_info_index& paired_index, paired_info_index& clustered_index) {
	exec_late_pair_info_count(gp, paired_index);
	INFO("STAGE == Estimating Distance");

	if (cfg::get().paired_mode) {
		if (cfg::get().advanced_estimator_mode) {
			AdvancedDistanceEstimator<Graph> estimator(gp.g, paired_index,
					gp.int_ids, cfg::get().ds.IS, cfg::get().ds.RL,
					cfg::get().de.delta, cfg::get().de.linkage_distance,
					cfg::get().de.max_distance, cfg::get().ade.threshold,
					cfg::get().ade.range_coeff, cfg::get().ade.delta_coeff,
					cfg::get().ade.cutoff, cfg::get().ade.minpeakpoints,
					cfg::get().ade.inv_density, cfg::get().ade.percentage,
					cfg::get().ade.derivative_threshold);

			estimator.Estimate(clustered_index);
		} else {
			//todo remove
//            stream.reset();
//            int e1 = 1065;
//            int e2 = 1158;
//            cout << "Edge 1 " << gp.int_ids.ReturnEdgeId(e1) << endl;
//            cout << "Edge 2 " << gp.int_ids.ReturnEdgeId(e2) << endl;
//            cout << "K " << (gp.g.EdgeNucls(gp.int_ids.ReturnEdgeId(e2))) << endl;
//            cout << "ThreadedPairedReadCount = " << ThreadedPairedReadCount<K + 1>(gp, e1, e2, stream) << endl;
//            cout << "TotalPositiveWeight = " << TotalPositiveWeight(gp, paired_index, e1, e2) << endl;

			INFO("Estimating distances");
			DistanceEstimator<Graph> estimator(gp.g, paired_index, gp.int_ids,
					cfg::get().ds.IS, cfg::get().ds.RL, cfg::get().de.delta,
					cfg::get().de.linkage_distance, cfg::get().de.max_distance);

			paired_info_index raw_clustered_index(gp.g);
			estimator.Estimate(raw_clustered_index);
			INFO("Distances estimated");

			INFO("Normalizing weights");
			//todo reduce number of constructor params
			PairedInfoWeightNormalizer<Graph> weight_normalizer(gp.g,
					cfg::get().ds.IS, cfg::get().ds.RL, debruijn_graph::K);
			PairedInfoNormalizer<Graph>
					normalizer(
							raw_clustered_index, /*&TrivialWeightNormalization<Graph>*/
							boost::bind(
									&PairedInfoWeightNormalizer<Graph>::NormalizeWeight,
									&weight_normalizer, _1));

			paired_info_index normalized_index(gp.g);
			normalizer.FillNormalizedIndex(normalized_index);
			INFO("Weights normalized");

			INFO("Filtering info");
			//todo add coefficient dependent on coverage and K
			PairInfoFilter<Graph> filter(gp.g, cfg::get().de.filter_threshold);
			filter.Filter(normalized_index, clustered_index);
			INFO("Info filtered");
			//		PairInfoChecker<Graph> checker(gp.edge_pos, 5, 100);
			//		checker.Check(raw_clustered_index);
			//		checker.WriteResults(cfg::get().output_dir + "/paired_stats");
		}

		//experimental
		INFO("Pair info aware ErroneousConnectionsRemoval");
		RemoveEroneousEdgesUsingPairedInfo(gp, paired_index);
		INFO("Pair info aware ErroneousConnectionsRemoval stats");
		CountStats<K>(gp.g, gp.index, gp.genome);
		//experimental
	}
}

void load_distance_estimation(conj_graph_pack& gp,
		paired_info_index& paired_index, paired_info_index& clustered_index,
		files_t* used_files) {
	fs::path p = fs::path(cfg::get().load_from) / "distance_estimation";
	used_files->push_back(p);

	ScanConjugateGraphPack(p.string(), gp, &paired_index, &clustered_index);
}

void save_distance_estimation(conj_graph_pack& gp,
		paired_info_index& paired_index, paired_info_index& clustered_index) {
	fs::path p = fs::path(cfg::get().output_saves) / "distance_estimation";
	PrintConjugateGraphPack(p.string(), gp, &paired_index, &clustered_index);
}

void count_estimated_info_stats(conj_graph_pack& gp,
		paired_info_index& paired_index, paired_info_index& clustered_index) {
	paired_info_index etalon_paired_index(gp.g);
	FillEtalonPairedIndex<debruijn_graph::K> (etalon_paired_index, gp.g,
			gp.index, gp.kmer_mapper, gp.genome);
	//todo temporary
	DataPrinter<Graph> data_printer(gp.g, gp.int_ids);
	data_printer.savePaired(cfg::get().output_dir + "etalon_paired",
			etalon_paired_index);
	//temporary
	CountClusteredPairedInfoStats(gp.g, gp.int_ids, paired_index, clustered_index,
			etalon_paired_index, cfg::get().output_dir);
}

void exec_distance_estimation(conj_graph_pack& gp,
		paired_info_index& paired_index, paired_info_index& clustered_index) {
	if (cfg::get().entry_point <= ws_distance_estimation) {
		estimate_distance(gp, paired_index, clustered_index);
		save_distance_estimation(gp, paired_index, clustered_index);
       if(cfg::get().paired_mode)
           count_estimated_info_stats(gp, paired_index, clustered_index);
	} else {
		INFO("Loading Distance Estimation");

		files_t used_files;
		load_distance_estimation(gp, paired_index, clustered_index, &used_files);
		copy_files(used_files);
	}
}

}
