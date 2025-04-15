// Print to terminal functions

void DiveStep::printStepDetails(const int step) const {
    std::cout << "| Step | Comp | Depth | Depth | P_amb |   GF  | pp_GF_n2 | pp_n2 | pp_GF_he | pp_he | pp_GF_inert | pp_inert |" << std::endl;
    
    for (int j = 0; j < NUM_COMPARTMENTS; j++) {
        std::cout << "|  " << std::setw(3) << step << " |  " 
                  << std::setw(3) << j + 1 << " |  " 
                  << std::fixed << std::setprecision(0)
                  << std::setw(3) << m_startDepth << "  | " 
                  << std::fixed << std::setprecision(0)
                  << std::setw(3) << m_endDepth << "  | " 
                  << std::fixed << std::setprecision(2)
                  << std::setw(5) << m_pAmbStartDepth << " | " 
                  << std::fixed << std::setprecision(0)
                  << std::setw(4) << m_gf << "  |     " 
                  << std::fixed << std::setprecision(2)
                  << std::setw(5) << m_ppMaxAdjustedGF[j].m_pN2 << "| " 
                  << std::setw(5) << m_ppActual[j].m_pN2 << " |     " 
                  << std::setw(5) << m_ppMaxAdjustedGF[j].m_pHe << "| " 
                  << std::setw(5) << m_ppActual[j].m_pHe << " |        " 
                  << std::setw(5) << m_ppMaxAdjustedGF[j].m_pInert << "| " 
                  << std::setw(5) << m_ppActual[j].m_pInert << "    |" 
                  << std::setw(5) << m_o2Percent << "   /   " 
                  << std::setw(5) << m_hePercent << std::endl;
    }
}

void DiveStep::printCompartmentDetails(const int step, const int compartment) const {
    std::cout << "|  " << std::setw(3) << step << " |  " 
              << std::setw(3) << compartment << " |  " 
              << std::fixed << std::setprecision(0)
              << std::setw(3) << m_endDepth << "  | " 
              << std::fixed << std::setprecision(2)
              << std::setw(5) << m_pAmbStartDepth << " | " 
              << std::fixed << std::setprecision(0)
              << std::setw(4) << m_gf << "  |     " 
              << std::fixed << std::setprecision(2)
              << std::setw(5) << m_ppMaxAdjustedGF[compartment].m_pN2 << "| " 
              << std::setw(5) << m_ppActual[compartment].m_pN2 << " |     " 
              << std::setw(5) << m_ppMaxAdjustedGF[compartment].m_pHe << "| " 
              << std::setw(5) << m_ppActual[compartment].m_pHe << " |        " 
              << std::setw(5) << m_ppMaxAdjustedGF[compartment].m_pInert << "| " 
              << std::setw(5) << m_ppActual[compartment].m_pInert << "    |" 
              << std::setw(5) << m_o2Percent << "   /   " 
              << std::setw(5) << m_hePercent << std::endl;
}

void DivePlan::printPlan(std::vector<DiveStep> profile){
    printf("\nDIVE PROFILE\n\n");
 
    // Print the header
    printf("-----------------------------------------------------------------------------------------------------------------------------------------------------------\n");
    printf("|Step| Phase|    Depth   | Ceil |  time /   run | Pamb / ppO2 |   GF  | GFSurf | O2 /  He /  N2  | SAC/ Amb /  Tot |  d  |    END (m)  |   CNS (%%)  | OTU |\n");
    printf("|  # |      |     (m)    |  (m) |      (min)    |  max (bar)  |       |        |      (%%)        | (L/min)  /  (L) |(g/L)| non O2 / O2 | Dive | Day | min |\n");
    printf("-----------------------------------------------------------------------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < (int) profile.size(); i++){
        printf("|%3i | ", i);
        if (profile[i].m_phase == Phase::DESCENDING)          printf("DESC | ");
        if (profile[i].m_phase == Phase::GAS_SWITCH)          printf("GAS  | ");
        if (profile[i].m_phase == Phase::STOP)                printf("STOP | ");
        if (profile[i].m_phase == Phase::DECO)                printf("DECO | ");
        if (profile[i].m_phase == Phase::ASCENDING)           printf("ASC  | ");
        if (profile[i].m_phase == Phase::GROUPED_ASCENDING)   printf("ASC* | ");
        
        printf("%3.0f -> %3.0f |  %3.0f | %5.1f / %5.1f | %4.1f / %3.2f | %4.0f  |  %4.0f  | %3.0f / %3.0f / %3.0f | %2.0f / %3.0f / %4.0f | %3.1f |   %3.0f / %3.0f |  %3.0f | %3.0f | %3.0f |\n", 
            profile[i].m_startDepth, profile[i].m_endDepth,
            profile[i].m_ceiling,
            profile[i].m_time, profile[i].m_runTime, 
            profile[i].m_pAmbMax, profile[i].m_pO2Max,
            profile[i].m_gf, profile[i].m_gfSurface,
            profile[i].m_o2Percent, profile[i].m_hePercent, profile[i].m_n2Percent, 
            profile[i].m_sacRate, profile[i].m_ambConsumptionAtDepth, profile[i].m_stepConsumption,
            profile[i].m_gasDensity, profile[i].m_endWithoutO2, profile[i].m_endWithO2,
            profile[i].m_cnsTotalSingleDive, profile[i].m_cnsTotalMultipleDives, profile[i].m_otuTotal);
    }

    printf("-----------------------------------------------------------------------------------------------------------------------------------------------------------\n");
}

void DivePlan::printCompartmentDetails(int compartment){
    printf("| Step | Comp | Depth | P_amb |   GF  | pp_GF_n2 | pp_n2 | pp_GF_he | pp_he | pp_GF_inert | pp_inert | O2   /   He \n");

    for (int i = 0; i < nbOfSteps(); i++){
        m_diveProfile[i].printCompartmentDetails(i, compartment);
    }
}

void DivePlan::printGF(){
    for (int i = 0; i < nbOfSteps(); i++){
        printf("Step: %3i | Depth: %3.0f | GF: %3.0f%%\n", i, m_diveProfile[i].m_endDepth, m_diveProfile[i].m_gf);
    }
}

void DivePlan::printO2Exposure(){
    printf("----------------------------------------------------------------------------------------------------\n");
    printf("| Step | Time | ppO2_max |  CNS Single |  CNS%% Step |  CNS Daily |  CNS%% Step | OTU/min | OTU Step |\n");
    printf("----------------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < nbOfSteps(); i++){
        printf("| %3i  |  %3.0f |   %4.2f   |      %3.0f    |    %5.1f   |     %3.0f    |   %5.1f    |  %4.1f   |    %5.1f |\n", 
            i, m_diveProfile[i].m_time, m_diveProfile[i].m_pO2Max, 
            m_diveProfile[i].m_cnsMaxMinSingleDive, m_diveProfile[i].m_cnsStepSingleDive,
            m_diveProfile[i].m_cnsMaxMinMultipleDives, m_diveProfile[i].m_cnsStepMultipleDives,
            m_diveProfile[i].m_otuPerMin, m_diveProfile[i].m_otuStep);
    }
    printf("----------------------------------------------------------------------------------------------------\n");
    printf(" Total                                      %5.1f                    %5.1f                   %5.1f\n",
        m_diveProfile[nbOfSteps() - 1].m_cnsTotalSingleDive, m_diveProfile[nbOfSteps() - 1].m_cnsTotalMultipleDives, 
        m_diveProfile[nbOfSteps() - 1].m_otuTotal);
}

void DivePlan::printSummary(){
    std::pair<double, double> result = getMaxTimeAndTTS();

    std::cout << "Dive Number: " << m_diveNumber << std::endl;
    
    std::cout << "GF " << g_parameters.m_gf[0] << " / " << g_parameters.m_gf[1] << std::endl;
    std::cout << "TTS Target: " << getTTS() << std::endl;
    std::cout << "TTS Max: " << result.second << " Max Time: " << result.first << std::endl;
    std::cout << "deltaTTS +5 min: " << getTTSDelta(5) << std::endl;
    
    // only if the dive is in OC or CC mode + m_bailout is true
    if ((m_mode == diveMode::OC || m_mode == diveMode::CC) && m_bailout){
        std::cout << "AP: " << getAP() << std::endl;
    }

    // only if m_mission is not 0
    if (m_mission != 0){
        std::cout << "Mission: " << m_mission << std::endl;
        std::cout << "T-TTS: " << getTurnTTS() << std::endl;

        // only if mode is OC
        if (m_mode == diveMode::OC){
            std::cout << "TP: " << getTP() << std::endl;
        }
    }

}

void GasList::print() {
    for (const Gas& gas : pImpl->gases) {
        std::cout << "Gas: " << gas.m_o2Percent << "%, " << gas.m_hePercent << "%" << std::endl;
    }
}
