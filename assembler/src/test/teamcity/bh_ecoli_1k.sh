#!/bin/bash

############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

set -e
pushd ../../../
rm -rf spades_output/ECOLI_1K_TEAMCITY_BH
./spades.py -o spades_output/ECOLI_1K_TEAMCITY_BH src/test/teamcity/bh_config_ecoli_1k.info
python src/tools/reads_utils/reads_quality.py -r test_dataset/reference_1K.fa.gz -o spades_output/ECOLI_1K_TEAMCITY_BH/reads_quality_results spades_output/ECOLI_1K_TEAMCITY_BH/corrected/ECOLI_1K_TEAMCITY_BH.dataset
python src/test/teamcity/assess_reads.py spades_output/ECOLI_1K_TEAMCITY_BH/reads_quality_results/all.tsv 100 100
popd
