0. Install combine (first time only)
====

Download a particular CMSSW release (currently, CMSSW_7_4_7)

	 export SCRAM_ARCH=slc6_amd64_gcc491
	 cmsrel CMSSW_7_4_7
	 cd CMSSW_7_4_7/src 
	 cmsenv

Clone HiggsAnalysis latino repository, update and compile

      git clone https://github.com/cms-analysis/HiggsAnalysis-CombinedLimit.git HiggsAnalysis/CombinedLimit
      cd HiggsAnalysis/CombinedLimit
      
      git fetch origin
      git checkout v6.2.1
      scramv1 b clean 
      scramv1 b

1. Create the datacard, following the latinos repository instructions
====

First, run mkShape.py and then mkDatacards.py

	https://github.com/latinos/PlotsConfigurations/blob/master/Configurations/ggH/README.md

To be able to use combine later, you have to run mkShape.py on at least one signal MC files, one background MC
and one data file.

2. Run combine
====

Go to the directory where you downloaded CMSW_7_4_7 and do

   cmsenv

and go back to your original directory, where you ran mkShape. There, execute the following command

     combine -M Asymptotic datacards/<your_path_to_datacard.txt>

This will create one rootfile for each candidate mass that can be read in the following step.

3. Read the rootfiles
====

You then need to run the macro plot_Asymptotic_ForCombination.C. The first step to do so is to modify
the file xsec_<your_analysis>.txt to include the official theoretical cross-section corresponding to
the different mass candidates. 

Then, run the macro in order to directly get the limits plot.

      root -l plot_Asymptotic_ForCombination.C