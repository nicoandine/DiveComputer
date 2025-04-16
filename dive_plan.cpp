#include "dive_plan.hpp"
#include <random>


namespace DiveComputer {

DivePlan::DivePlan(double depth, double time, diveMode mode, int diveNumber, std::vector<CompartmentPP> initialPressure){
    m_diveNumber = diveNumber;
    m_mode = mode;

    m_initialPressure = initialPressure;
    
    m_stopSteps.clear();
    m_stopSteps.addStopStep(depth, time);

    // Load available gases before building the dive plan
    loadAvailableGases();

    // Build the dive plan
    buildDivePlan();
}

// Core methods
void DivePlan::loadAvailableGases() {
    m_gasAvailable.clear();
    
    for (const auto& gas : g_gasList.getGases()) {
        // Only add active gases
        if (gas.m_gasStatus == GasStatus::ACTIVE) {
            m_gasAvailable.emplace_back(gas);
        }
    }
    
    // Ensure we have at least one gas (air as default)
    if (m_gasAvailable.empty()) {
        Gas defaultGas(g_constants.m_oxygenInAir, 0.0, GasType::BOTTOM, GasStatus::ACTIVE);
        m_gasAvailable.emplace_back(defaultGas);
    }
}

void DivePlan::buildDivePlan(){
    // Log performance
    QElapsedTimer timer;
    timer.start();

    clear();

    // Set mode
    stepMode activeMode = (m_mode == diveMode::CC) ? stepMode::CC : stepMode::OC;

    // Add initial surface step
    auto& surfaceStep = addStep(0, 0, 0, Phase::STOP, activeMode);
    surfaceStep.m_ppActual = m_initialPressure;

    if (m_stopSteps.nbOfStopSteps() == 0) return;

    // Sort and get deepest stop
    m_stopSteps.sortDescending();
    double maxDepth = m_stopSteps.m_stopSteps[0].m_depth;

    // Add descending phase and initial deep stop
    addStep(0.0, 0.0, 0, Phase::GAS_SWITCH, activeMode);
    addStep(0.0, maxDepth, maxDepth / g_parameters.m_maxDescentRate, Phase::DESCENDING, activeMode);
    addStep(maxDepth, maxDepth, m_stopSteps.m_stopSteps[0].m_time, Phase::STOP, activeMode);
    
    // Collect all stops in one pass
    std::set<double> allStops = { maxDepth, 0.0 };
    
    // Add planned stops
    for (int i = 1; i < m_stopSteps.nbOfStopSteps(); i++) {
        allStops.insert(m_stopSteps.m_stopSteps[i].m_depth);
    }
    
    // Add required intermediate stops at multiples of m_depthIncrement
    for (double depth = calculateFirstStopDepth(maxDepth); 
         depth >= g_parameters.m_lastStopDepth; 
         depth -= g_parameters.m_depthIncrement) {
        if (std::abs(depth - maxDepth) > 0.1) {  // Skip if already added deepest stop
            allStops.insert(depth);
        }
    }
    
    // Add last stop depth if needed
    allStops.insert(g_parameters.m_lastStopDepth);
    
    // Convert to vector and sort descending
    std::vector<double> ascentStops(allStops.begin(), allStops.end());
    std::sort(ascentStops.begin(), ascentStops.end(), std::greater<double>());
    
    // Build the profile
    processAscentStops(ascentStops);

    // Initialise the ppActual for Step 0
    m_diveProfile[0].m_ppActual = m_initialPressure;

    // Monitor performance
    logWrite("DivePlan::build() took ", timer.elapsed(), " ms");
}

void DivePlan::calculateDivePlan(bool printLog) {
    if (m_diveProfile.empty()) return;

    // Log performance
    QElapsedTimer timer;
    timer.start();
    if (printLog) {
        logWrite("DivePlan::calculate() - START");
    }

    m_firstDecoDepth = 0;

    // First pass at the deco plan
    updatePpAmb();
    clearDecoSteps();
    updateStepsPhaseFromFirstDeco();
    applyGases();
    applyGF();
    calculatePPInertGas();
    calculatePPInertGasMax();

    // Returns the first deco stop, required for applying the GF
    setFirstDecoDepth();

    // Update phase from first deco
    // Re-apply gases after the first deco is found as the maxPPo2 will have changed for deco steps
    // Re-calculate ppInertGas for all steps after re-applying the gas
    // Apply the gradient factor to each step based on first deco stop determined
    // Re-calculate the pp_max values for each step adjusted for the GF
    updateStepsPhaseFromFirstDeco();
    applyGases();
    calculatePPInertGas();
    applyGF();
    calculatePPInertGasMax();   

    // Calculate deco steps & Update other variables
    calculateDecoSteps();
    updateStepsPhaseFromFirstDeco();
    calculateOtherVariables(100, printLog); // GF 100 for ceiling
    calculateTimeProfile(printLog);

    // Monitor performance
    if (printLog) {
        logWrite("DivePlan::calculate() took ", timer.elapsed(), " ms");
        logWrite("DivePlan::calculate() - END");
    }
}

void DivePlan::calculateOtherVariables(double GF, bool printLog){
    // Log performance
    QElapsedTimer timer;
    timer.start();

    updateStepsPhaseFromFirstDeco();

    for (int i = 0; i < nbOfSteps(); i++){
        m_diveProfile[i].updatePAmb();
        m_diveProfile[i].updateCeiling(GF);        
        m_diveProfile[i].updateConsumption();
        m_diveProfile[i].m_pO2Max = m_diveProfile[i].m_pAmbMax * m_diveProfile[i].m_o2Percent / 100.0;
        m_diveProfile[i].m_n2Percent = 100.0 - m_diveProfile[i].m_o2Percent - m_diveProfile[i].m_hePercent;
        m_diveProfile[i].updateGFSurface(&m_diveProfile[nbOfSteps() - 1]);
        m_diveProfile[i].updateDensity();
        m_diveProfile[i].updateEND();

        if (i > 0){
            m_diveProfile[i].updateOxygenToxicity(&m_diveProfile[i - 1]);
            m_diveProfile[i].updateRunTime(&m_diveProfile[i - 1]);
        }
        else{
            m_diveProfile[0].m_runTime = m_diveProfile[0].m_time;
        }
    }

    // Monitor performance
    if (printLog) {
        logWrite("DivePlan::updateVariables() took ", timer.elapsed(), " ms");
    }
}

void DivePlan::calculateTimeProfile(bool printLog){
    // Log performance
    QElapsedTimer timer;
    timer.start();
    
    double time_increment = g_parameters.m_timeIncrementDeco;
    int total_time_steps = (int) (m_diveProfile[nbOfSteps() - 1].m_runTime / time_increment);

    m_timeProfile.clear();
    m_timeProfile.resize(total_time_steps);

    int diveplan_index = 0;
    int timeplan_index = 0;
        
    double run_time = time_increment;
    double CNS_total_single_dive = 0;
    double CNS_total_multiple_dives = 0;
    double OTU_total = 0;

    for (diveplan_index = 1; diveplan_index < nbOfSteps(); diveplan_index++){
        double diveplan_start_time = m_diveProfile[diveplan_index].m_runTime - m_diveProfile[diveplan_index].m_time;
        double diveplan_end_time = m_diveProfile[diveplan_index].m_runTime;

        while (diveplan_start_time < run_time && run_time <= diveplan_end_time){
            m_timeProfile[timeplan_index] = m_diveProfile[diveplan_index];
            
            // Only adjusts the values which are dependant on time
            m_timeProfile[timeplan_index].m_time = time_increment;
            m_timeProfile[timeplan_index].m_runTime = run_time;

            m_timeProfile[timeplan_index].m_stepConsumption = m_timeProfile[timeplan_index].m_ambConsumptionAtDepth * time_increment;
            
            m_timeProfile[timeplan_index].m_cnsStepSingleDive = 
                (m_timeProfile[timeplan_index].m_cnsMaxMinSingleDive != 0) ? 100 * time_increment / m_timeProfile[timeplan_index].m_cnsMaxMinSingleDive : 0;
            CNS_total_single_dive += m_timeProfile[timeplan_index].m_cnsStepSingleDive;
            m_timeProfile[timeplan_index].m_cnsTotalSingleDive = CNS_total_single_dive;

            m_timeProfile[timeplan_index].m_cnsStepMultipleDives = 
                (m_timeProfile[timeplan_index].m_cnsMaxMinMultipleDives != 0) ? 100 * time_increment / m_timeProfile[timeplan_index].m_cnsMaxMinMultipleDives : 0;
            CNS_total_multiple_dives += m_timeProfile[timeplan_index].m_cnsStepMultipleDives;
            m_timeProfile[timeplan_index].m_cnsTotalMultipleDives = CNS_total_multiple_dives;

            m_timeProfile[timeplan_index].m_otuStep = time_increment * m_timeProfile[timeplan_index].m_otuPerMin;
            OTU_total += m_timeProfile[timeplan_index].m_otuStep;
            m_timeProfile[timeplan_index].m_otuTotal = OTU_total;

            double pp_time = run_time - (m_diveProfile[diveplan_index].m_runTime - m_diveProfile[diveplan_index].m_time);
            m_timeProfile[timeplan_index].calculatePPInertGasForStep(m_diveProfile[diveplan_index], pp_time);

            timeplan_index++;
            run_time += time_increment;
        }
    }

    for (int i = 0; i < total_time_steps; i++){
        m_timeProfile[i].updateGFSurface(&m_diveProfile[nbOfSteps() - 1]);
        m_timeProfile[i].updateCeiling(100);
    }

    // Monitor performance
    if (printLog) {
        logWrite("DivePlan::updateTimeProfile() took ", timer.elapsed(), " ms");
    }

}

void DivePlan::calculateGasConsumption(bool printLog) {
    if (m_gasAvailable.empty() || m_diveProfile.empty()) {
        return;
    }
    
    // Log performance
    QElapsedTimer timer;
    timer.start();

    // Reset consumption for all gases
    for (auto& gas : m_gasAvailable) {
        gas.m_consumption = 0.0;
    }
    
    // Calculate consumption for each step
    for (const auto& step : m_diveProfile) {
        // Find the matching gas for this step
        for (auto& gas : m_gasAvailable) {
            if (std::abs(gas.m_gas.m_o2Percent - step.m_o2Percent) < 0.1 && 
                std::abs(gas.m_gas.m_hePercent - step.m_hePercent) < 0.1) {
                // Add this step's consumption to the gas
                gas.m_consumption += step.m_stepConsumption;
                break;
            }
        }
    }
    
    // Calculate end pressure for each gas
    for (auto& gas : m_gasAvailable) {
        if (gas.m_nbTanks > 0 && gas.m_tankCapacity > 0) {
            gas.m_endPressure = gas.m_fillingPressure - (gas.m_consumption / (gas.m_nbTanks * gas.m_tankCapacity));
        } else {
            gas.m_endPressure = 0.0;
        }
    }

    // Monitor performance
    if (printLog) {
        logWrite("DivePlan::updateGasConsumption() took ", timer.elapsed(), " ms");
    }
}

void DivePlan::calculateDiveSummary(bool printLog) {
    if (m_diveProfile.empty()) return;

    if (printLog) {
        logWrite("DivePlan::calculateDiveSummary() - START");
    }

    // Log performance
    QElapsedTimer timer;
    timer.start();
    
    m_tts = getTTS();
    m_ttsDelta = getTTSDelta(5);
   
    bool showAP = (m_mode == diveMode::OC) || 
                  (m_mode == diveMode::CC && m_bailout);


    if (showAP) {
        m_maxResult = getMaxTimeAndTTS();
        m_ap = getAP();
    }

    bool hasMission = (m_mission > 0);
    if (hasMission){
        m_turnTts = getTurnTTS();
    }
    
    bool showTP = (m_mode == diveMode::OC && hasMission);
    if (showTP){
        m_tp = getTP();
    }
    
    // Monitor performance
    if (printLog) {
        logWrite("DivePlan::calculateDiveSummary() took ", timer.elapsed(), " ms");
        logWrite("DivePlan::calculateDiveSummary() - END");
    }
}

void DivePlan::clearDecoSteps(){
    for (int i = 0; i < nbOfSteps(); i++){
        if (m_diveProfile[i].m_phase == Phase::DECO){
            m_diveProfile[i].m_time = 0.0;
        }
    }
}

bool DivePlan::enoughGasAvailable(){
    for (auto& gas : m_gasAvailable){
        if (gas.m_endPressure < gas.m_reservePressure){
            return false;
        }
    }
    return true;
}

void DivePlan::optimiseDecoGas() {
   // placeholder used for testing for now
}

double DivePlan::getTTS(){
    double endBottomTime = 0.0;
    double endDiveTime = m_diveProfile[nbOfSteps() - 1].m_runTime;
    double TTS = 0;

    for (int i = 1; i < nbOfSteps(); i++){
        if (m_diveProfile[i].m_phase == Phase::STOP){
            endBottomTime = m_diveProfile[i].m_runTime;
            break;
        }
    }

    TTS = (endDiveTime - endBottomTime);
    return TTS;
}

double DivePlan::getTTSDelta(double incrementTime){
    // Log performance
    QElapsedTimer timer;
    timer.start();

    DivePlan tempDivePlan = *this;

    // Find the deepest STOP phase in the dive profile
    int deepestStopIndex = -1;
    double deepestDepth = 0.0;
    
    for (int i = 0; i < nbOfSteps(); i++) {
        if (tempDivePlan.m_diveProfile[i].m_phase == Phase::STOP && 
            tempDivePlan.m_diveProfile[i].m_endDepth > deepestDepth) {
            deepestStopIndex = i;
            deepestDepth = tempDivePlan.m_diveProfile[i].m_endDepth;
        }
    }

    // If no STOP phase was found, return 0
    if (deepestStopIndex == -1) {
        return 0.0;
    }

    // Adjust the duration of the deepest step
    tempDivePlan.m_diveProfile[deepestStopIndex].m_time = std::max(0.0, 
        tempDivePlan.m_diveProfile[deepestStopIndex].m_time + incrementTime);

    // Recalculate the dive plan
    tempDivePlan.calculateDivePlan(false);

    // Monitor performance
    logWrite("DivePlan::getTTSDelta() took ", timer.elapsed(), " ms");

    return tempDivePlan.getTTS() - getTTS();
}

std::pair<double, double> DivePlan::getMaxTimeAndTTS() {       
    // Log performance
    QElapsedTimer timer;
    timer.start();

    DivePlan tempDivePlan = *this;
    double maxTime = 0.0, maxTTS = 0.0;

    // find the first STOP step
    int firstStopIndex = 0;

    for (int i = 1; i < nbOfSteps(); i++){
        if (tempDivePlan.m_diveProfile[i].m_phase == Phase::STOP){
            tempDivePlan.m_diveProfile[i].m_time = 0.0;
            firstStopIndex = i;
            break;
        }
    }

    // Recalculate the dive plan
    tempDivePlan.calculateDivePlan(false);
    tempDivePlan.calculateGasConsumption(false);

    // Check if 0 of bottom time is enough gas available
    if(!tempDivePlan.enoughGasAvailable()){
        return std::make_pair(0.0, 0.0);
    }

    // iterate the time of the bottom phase until the gas is not enough
    while(tempDivePlan.enoughGasAvailable()){
        tempDivePlan.m_diveProfile[firstStopIndex].m_time += g_parameters.m_timeIncrementMaxTime;
        tempDivePlan.calculateDivePlan(false);
        tempDivePlan.calculateGasConsumption(false);
    }

    tempDivePlan.m_diveProfile[firstStopIndex].m_time -= g_parameters.m_timeIncrementMaxTime;
    maxTime = std::max(0.0, tempDivePlan.m_diveProfile[firstStopIndex].m_time);
    tempDivePlan.calculateDivePlan(false);
    maxTTS = tempDivePlan.getTTS();

    // Monitor performance
    logWrite("DivePlan::getMaxTimeAndTTS() took ", timer.elapsed(), " ms");

    return std::make_pair(maxTime, maxTTS);
}

// Function to calculate the turn pressure
double DivePlan::getTP() {
    // If no mission time is set, return the ascent pressure
    if (m_mission <= 0) {
        return getAP();
    }
    
    // Find the deepest STOP phase in the dive profile
    int deepestStopIndex = -1;
    double deepestDepth = 0.0;
    
    for (int i = 0; i < nbOfSteps(); i++) {
        if (m_diveProfile[i].m_phase == Phase::STOP && 
            m_diveProfile[i].m_endDepth > deepestDepth) {
            deepestStopIndex = i;
            deepestDepth = m_diveProfile[i].m_endDepth;
        }
    }
    
    // If no STOP phase was found, return 0
    if (deepestStopIndex == -1) {
        return 0;
    }
    
    // Get the deepest stop step
    const DiveStep& deepestStop = m_diveProfile[deepestStopIndex];
    
    // Find the gas being used at the deepest stop
    const GasAvailable* matchingGas = nullptr;
    for (const auto& gas : m_gasAvailable) {
        if (std::abs(gas.m_gas.m_o2Percent - deepestStop.m_o2Percent) < 0.1 && 
            std::abs(gas.m_gas.m_hePercent - deepestStop.m_hePercent) < 0.1) {
            matchingGas = &gas;
            break;
        }
    }
    
    // If no matching gas found, return the standard ascent pressure
    if (!matchingGas) {
        return 0;
    }
    
    // Calculate ambient pressure at the deepest stop depth
    double ambientPressure = getPressureFromDepth(deepestDepth);
    
    // Determine SAC rate based on mode
    double sacRate = deepestStop.m_sacRate;
    
    // Calculate gas consumption rate at depth
    double consumptionRate = sacRate * ambientPressure;
    
    // Calculate total gas used during mission time
    double gasUsedDuringMission = consumptionRate * m_mission;
    
    // Calculate pressure drop based on tank configuration
    double pressureDrop = 0.0;
    if (matchingGas->m_nbTanks > 0 && matchingGas->m_tankCapacity > 0) {
        pressureDrop = gasUsedDuringMission / (
            (g_parameters.m_calculateAPandTPonOneTank ? 1 : matchingGas->m_nbTanks) * matchingGas->m_tankCapacity);
    }
    
    // Turn pressure = ascent pressure + mission pressure requirement
    return getAP() + pressureDrop;
}

double DivePlan::getTurnTTS() {
    // If no mission time is set, return the standard TTS
    if (m_mission <= 0) {
        return getTTS();
    }

    // uses the delta TTS function
    double deltaTTS = getTTSDelta(-m_mission);

    return getTTS() + deltaTTS;

}

double DivePlan::getAP() {
    // Find the first ascent step after the bottom phase
    int firstAscentIndex = -1;
    for (int i = 0; i < nbOfSteps(); i++) {
        if (m_diveProfile[i].m_phase == Phase::ASCENDING) {
            firstAscentIndex = i;
            break;
        }
    }
    
    // If no ascent step found, return 0
    if (firstAscentIndex == -1) {
        return 0.0;
    }
    
    // Get the initial gas mix (O2/He percentages)
    double initialO2 = m_diveProfile[firstAscentIndex].m_o2Percent;
    double initialHe = m_diveProfile[firstAscentIndex].m_hePercent;
    
    // Sum gas consumption until a gas switch occurs
    double totalConsumption = 0.0;
    int i = firstAscentIndex;
    
    while (i < nbOfSteps()) {
        // Check if we reached a gas switch
        if (i > firstAscentIndex && 
            (std::abs(m_diveProfile[i].m_o2Percent - initialO2) > 0.1 || 
             std::abs(m_diveProfile[i].m_hePercent - initialHe) > 0.1)) {
            break;
        }
        
        // Add consumption for this step
        totalConsumption += m_diveProfile[i].m_stepConsumption;
        i++;
    }
    
    // Find the matching gas in available gases
    const GasAvailable* matchingGas = nullptr;
    for (const auto& gas : m_gasAvailable) {
        if (std::abs(gas.m_gas.m_o2Percent - initialO2) < 0.1 && 
            std::abs(gas.m_gas.m_hePercent - initialHe) < 0.1) {
            matchingGas = &gas;
            break;
        }
    }
    
    // If no matching gas found, return 0
    if (!matchingGas) {
        return 0.0;
    }
    
    // Calculate pressure consumption based on tanks and capacity
    double pressureConsumed = 0.0;
    if (matchingGas->m_nbTanks > 0 && matchingGas->m_tankCapacity > 0) {
        pressureConsumed = totalConsumption / (
            (g_parameters.m_calculateAPandTPonOneTank ? 1 : matchingGas->m_nbTanks) * matchingGas->m_tankCapacity);
    }
    
    // Add reserve pressure to get the minimum starting pressure (AP)
    double ap = matchingGas->m_reservePressure + pressureConsumed;
    
    return ap;
}

double DivePlan::getNoFlyTime(){
    // Log performance
    QElapsedTimer timer;
    timer.start();

    DiveStep flyDive[3];

    // Initialise the step with the last step of the dive profile
    flyDive[0] = m_diveProfile[nbOfSteps() - 1];

    // Step to wait at the surface
    flyDive[1].m_pAmbStartDepth = g_parameters.m_atmPressure;
    flyDive[1].m_pAmbEndDepth = g_parameters.m_atmPressure;
    flyDive[1].m_pAmbMax = g_parameters.m_atmPressure;
    flyDive[1].m_time = 0;
    flyDive[1].m_n2Percent = 100 - g_constants.m_oxygenInAir;
    flyDive[1].m_hePercent = 0;
    flyDive[1].m_gf = g_parameters.m_noFlyGf;

    // Step in the plane
    flyDive[2].m_pAmbStartDepth = g_parameters.m_noFlyPressure;
    flyDive[2].m_pAmbEndDepth = g_parameters.m_noFlyPressure;
    flyDive[2].m_pAmbMax = g_parameters.m_noFlyPressure;
    flyDive[2].m_time = 0;
    flyDive[2].m_n2Percent = 100 - g_constants.m_oxygenInAir;
    flyDive[2].m_hePercent = 0;
    flyDive[2].m_gf = g_parameters.m_noFlyGf;

    for (int i = 0; i < 3; i++){
        double lastRatioN2He = 1.0; // ration n2/inert = 1
        flyDive[i].calculatePPInertGasMaxForStep(lastRatioN2He);
    }

    flyDive[1].calculatePPInertGasForStep(flyDive[0], flyDive[1].m_time);
    flyDive[2].calculatePPInertGasForStep(flyDive[1], flyDive[2].m_time);

    while (flyDive[2].getIfBreachingDecoLimits()){
        flyDive[1].m_time += g_parameters.m_noFlyTimeIncrement;
        flyDive[1].calculatePPInertGasForStep(flyDive[0], flyDive[1].m_time);
        flyDive[2].calculatePPInertGasForStep(flyDive[1], flyDive[2].m_time);
    }
 
    logWrite("DivePlan::getNoFlyTime() took ", timer.elapsed(), " ms");
 
    // Convert time to hours and round up to no decimal places
    return std::ceil(flyDive[1].m_time / 60.0);
}

// HELPER METHODS

void DivePlan::clear(){
    m_diveProfile.clear();
}

void DivePlan::sortGases(){
    // Sort gases by increasing O2 then increasing He
    std::sort(m_gasAvailable.begin(), m_gasAvailable.end(), [](const GasAvailable& a, const GasAvailable& b) {
        if (a.m_gas.m_o2Percent != b.m_gas.m_o2Percent) {
            return a.m_gas.m_o2Percent < b.m_gas.m_o2Percent;
        }
        return a.m_gas.m_hePercent < b.m_gas.m_hePercent;
    });
}

void DivePlan::applyGases() {
    if (m_gasAvailable.empty()) {
        return;
    }

    // Sort gases
    sortGases();

    // Initialise the gas switch depth and ppO2
    for (auto& gas : m_gasAvailable) {
        gas.m_switchDepth = 0.0;
        gas.m_switchPpO2 = 0.0;
    }

    // Delete all GAS_SWITCH steps
    std::vector<DiveStep> filteredSteps;
    for (const auto& step : m_diveProfile) {
        if (step.m_phase != Phase::GAS_SWITCH) {
            filteredSteps.push_back(step);
        }
    }
    m_diveProfile = filteredSteps;

    // Create a new profile vector to hold steps with gas switches
    std::vector<DiveStep> updatedProfile;
    updatedProfile.reserve(m_diveProfile.size() * 2); // Reserve space for potential new steps
    
    // Track previous step's gas to detect changes
    double prevO2Percent = g_constants.m_oxygenInAir;
    double prevHePercent = 0.0;
    stepMode prevMode = stepMode::CC;
    
    // Process each step and add gas switches in a single pass
    for (size_t i = 0; i < m_diveProfile.size(); ++i) {
        auto& step = m_diveProfile[i];
        const Gas* selectedGas = nullptr;
        double maxDepth = std::max(step.m_startDepth, step.m_endDepth);
            
        // Check for surface step
        if(std::abs(step.m_startDepth) < 0.1 && std::abs(step.m_endDepth) < 0.1) {
            Gas gas = Gas(); // Default to Air
            selectedGas = &gas;
            step.m_o2Percent = gas.m_o2Percent;
            step.m_hePercent = gas.m_hePercent;
        }
        else {
            // Determine the max ppO2 for this phase
            double maxppO2 = 0.0;
            switch(step.m_mode) {
                case stepMode::OC:
                    maxppO2 = g_parameters.m_PpO2Active;
                    break;
                case stepMode::BAILOUT:
                    maxppO2 = g_parameters.m_PpO2Active;
                    break;
                case stepMode::DECO:
                    maxppO2 = g_parameters.m_PpO2Deco;
                    break;
                case stepMode::CC:
                    maxppO2 = g_parameters.m_maxPpO2Diluent;
                    break;
                default:
                    maxppO2 = g_parameters.m_PpO2Active;
                    break;
            }
            // Find the gas with the smallest MOD that can be used at this depth    
            double smallestMOD = std::numeric_limits<double>::max();

            for (const auto& gas : m_gasAvailable) {
                double gasMOD = gas.m_gas.MOD(maxppO2);                

                // Check if the gas MOD is smaller than the smallest MOD and greater than or equal to the current depth
                if (gasMOD < smallestMOD && gasMOD >= maxDepth) {
                    smallestMOD = gasMOD;
                    selectedGas = &gas.m_gas;
                }
            }

            // If no gas available, use the first gas available (which has the lowest O2 content)
            if (selectedGas == nullptr) {
                // Try to use the first available gas as a default
                if (!m_gasAvailable.empty()) {
                    selectedGas = &m_gasAvailable[0].m_gas;
                } else {
                    // If no gas available, use a default air
                    static Gas defaultAir;
                    selectedGas = &defaultAir;
                }
            }

            step.m_pAmbMax = std::max(getPressureFromDepth(step.m_startDepth), getPressureFromDepth(step.m_endDepth));

            if(step.m_mode == stepMode::CC) {
                step.m_o2Percent = std::min(m_setPoints.getSetPointAtDepth(maxDepth, m_boosted) / step.m_pAmbMax * 100.0, 100.0);
                step.m_hePercent = (100 - step.m_o2Percent) * selectedGas->m_hePercent / (100 - selectedGas->m_o2Percent);
            }
            else{
                step.m_o2Percent = selectedGas->m_o2Percent;
                step.m_hePercent = selectedGas->m_hePercent;
            }
        }

        // Calculate N2 and other values for the step
        step.m_n2Percent = 100.0 - step.m_o2Percent - step.m_hePercent;
        step.m_pO2Max = (step.m_o2Percent / 100.0) * step.m_pAmbMax;

        // Check if we need to add a gas switch step
        if (i > 0 && !(step.m_mode == stepMode::CC && prevMode == stepMode::CC) &&
            (std::abs(step.m_o2Percent - prevO2Percent) > 0.1 || 
             std::abs(step.m_hePercent - prevHePercent) > 0.1)) {
            
            // Find the corresponding GasAvailable object
            for (auto& gasAvailable : m_gasAvailable) {
                if (&gasAvailable.m_gas == selectedGas) {
                    gasAvailable.m_switchDepth = std::max(step.m_startDepth, gasAvailable.m_switchDepth);
                    gasAvailable.m_switchPpO2 = std::max(step.m_pAmbMax * step.m_o2Percent / 100.0, gasAvailable.m_switchPpO2);
                    break;
                }
            }

            // Create a gas switch step
            DiveStep gasSwitch = step;
            gasSwitch.m_endDepth = step.m_startDepth;
            gasSwitch.m_time = 0.0; // Minimal time
            gasSwitch.m_phase = Phase::GAS_SWITCH; 
            
            // Use the gas of the current step (the new gas being switched to)
            
            // Add the gas switch step to the updated profile
            updatedProfile.push_back(gasSwitch);
        }
        
        // Add the current step to the updated profile
        updatedProfile.push_back(step);
        
        // Store current step's gas info for the next iteration
        prevO2Percent = step.m_o2Percent;
        prevHePercent = step.m_hePercent;
        prevMode = step.m_mode;
    }
    
    // Replace the original profile with the updated one
    m_diveProfile = std::move(updatedProfile);

}

bool DivePlan::getIfBreachingDecoLimitsInRange(int deco, int next_deco){
    for (int k = deco; k <= next_deco; k++){
        if (m_diveProfile[k].getIfBreachingDecoLimits()){
            return true;
        }
    }
    return false;
}

void DivePlan::calculatePPInertGasInRange(int deco, int next_deco){
    for (int k = deco; k <= next_deco; k++){
        m_diveProfile[k].calculatePPInertGasForStep(m_diveProfile[k - 1], m_diveProfile[k].m_time);
    }
}

void DivePlan::calculateDecoSteps(){
    int deco_index = 0, next_deco_index = 0;

    for (int i = 3; i < nbOfSteps() - 1; i++){
        if (m_diveProfile[i].m_phase == Phase::DECO){
            deco_index = i;

            // look for the next deco stop
            bool found_next_deco = false;
            for (int j = i + 1; j < nbOfSteps(); j++){
                if (m_diveProfile[j].m_phase == Phase::DECO){
                    next_deco_index = j;
                    found_next_deco = true;
                    break;
                }
            }

            if (!found_next_deco){
                next_deco_index = nbOfSteps() - 1;
            }

            while(getIfBreachingDecoLimitsInRange(deco_index, next_deco_index)){
                m_diveProfile[deco_index].m_time += g_parameters.m_timeIncrementDeco;
                calculatePPInertGasInRange(deco_index, next_deco_index);
            }
        }
    }
}

double DivePlan::calculateFirstStopDepth(double maxDepth){
    double firstStopDepth = std::ceil(maxDepth / g_parameters.m_depthIncrement) * g_parameters.m_depthIncrement;
    return (firstStopDepth > maxDepth) ? firstStopDepth - g_parameters.m_depthIncrement : firstStopDepth;
}

void DivePlan::processAscentStops(const std::vector<double>& ascentStops){
    stepMode ascentMode = (m_mode == diveMode::CC) ? (m_bailout ? stepMode::BAILOUT : stepMode::CC) : stepMode::OC;

    for (size_t i = 0; i < ascentStops.size() - 1; ++i) {
        double fromDepth = ascentStops[i];
        double toDepth = ascentStops[i+1];
        
        // Add ascending step
        double ascendTime = (fromDepth - toDepth) / g_parameters.m_maxAscentRate;
        addStep(fromDepth, toDepth, ascendTime, Phase::ASCENDING, ascentMode);
        
        // Check if this is a planned stop
        bool isPlannedStop = false;
        double stopTime = 0;
        
        for (const auto& stop : m_stopSteps.m_stopSteps) {
            if (std::abs(stop.m_depth - toDepth) < 0.1) {
                isPlannedStop = true;
                stopTime = stop.m_time;
                break;
            }
        }
        
        // Add appropriate step
        if (isPlannedStop) {
            addStep(toDepth, toDepth, stopTime, Phase::STOP, ascentMode);
            if (toDepth > 0) {
                addStep(toDepth, toDepth, 0, Phase::DECO, ascentMode);
            }
        } else if (toDepth > 0) {
            addStep(toDepth, toDepth, 0, Phase::DECO, ascentMode);
        } else {
            addStep(toDepth, toDepth, 0, Phase::STOP, ascentMode);
        }
    }
}

int DivePlan::nbOfSteps(){
    return m_diveProfile.size();
}

DiveStep& DivePlan::addStep(double start_depth, double end_depth, double time, Phase phase, stepMode mode){
    DiveStep step;
    step.m_startDepth = start_depth;
    step.m_endDepth = end_depth;
    step.m_time = time;
    step.m_phase = phase;
    step.m_mode = mode;
    m_diveProfile.push_back(step);
    return m_diveProfile.back();
}

DiveStep& DivePlan::insertStep(int index, double start_depth, double end_depth, double time, Phase phase, stepMode mode){
    DiveStep step;
    step.m_startDepth = start_depth;
    step.m_endDepth = end_depth;
    step.m_time = time;
    step.m_phase = phase;
    step.m_mode = mode;
    m_diveProfile.insert(m_diveProfile.begin() + index, step);
    return m_diveProfile[index];
}

void DivePlan::deleteStep(int index) {
    if (index >= 0 && index < static_cast<int>(m_diveProfile.size())) {
        m_diveProfile.erase(m_diveProfile.begin() + index);
    }
}

// Decompression methods

void DivePlan::calculatePPInertGas() {
    for (int i = 1; i < (int) m_diveProfile.size(); i++) {
        m_diveProfile[i].calculatePPInertGasForStep(m_diveProfile[i - 1], m_diveProfile[i].m_time);
    }
}

void DivePlan::calculatePPInertGasMax() {
    double lastRatioN2He = 1.0;

    for (int i = 1; i < (int) m_diveProfile.size(); i++) {
        m_diveProfile[i].calculatePPInertGasMaxForStep(lastRatioN2He);
    }
}

void DivePlan::applyGF() {
    for (int i = 1; i < (int) m_diveProfile.size(); i++) {
        m_diveProfile[i].m_gf = getGF(m_diveProfile[i].m_endDepth, m_firstDecoDepth);
    }
}

void DivePlan::setFirstDecoDepth() {
    for (int i = 3; i < nbOfSteps(); i++) {
        if (m_diveProfile[i].getIfBreachingDecoLimits()) {
            m_firstDecoDepth = m_diveProfile[i - 1].m_startDepth;
            break;
        }
    }
}

// Update variable functions

void DivePlan::updateStepsPhaseFromFirstDeco(){
    stepMode decoMode = (m_mode == diveMode::CC && !m_bailout) ? stepMode::CC : stepMode::DECO;

    for (int i = 0; i < (int) m_diveProfile.size(); i++) {
        if (m_diveProfile[i].m_phase == Phase::DECO && m_diveProfile[i].m_startDepth <= m_firstDecoDepth) {
            int j = i;
            while (j < (int) m_diveProfile.size() && m_diveProfile[j].m_phase != Phase::STOP) {
                m_diveProfile[j].m_mode = decoMode;
                j += 1;
            }
        }
    }
}

void DivePlan::updatePpAmb(){
    for (int i = 0; i < nbOfSteps(); i++){
        m_diveProfile[i].updatePAmb();
    }
}

void DivePlan::updateCeiling(double GF){
    for (int i = 0; i < nbOfSteps(); i++){
        m_diveProfile[i].updateCeiling(GF);
    }
}

void DivePlan::updateOxygenToxicity(){
    for (int i = 1; i < nbOfSteps(); i++) {
        m_diveProfile[i].updateOxygenToxicity(&m_diveProfile[i - 1]);
    }
}

void DivePlan::updateConsumptions(){
    for (int i = 0; i < nbOfSteps(); i++) {
        m_diveProfile[i].updateConsumption();
    }
}

void DivePlan::updateGFSurface(){
    
    for (int i = 0; i < (int) m_diveProfile.size(); i++) {
        m_diveProfile[i].updateGFSurface(&m_diveProfile[nbOfSteps() - 1]);
    }
}

void DivePlan::updateRunTimes(){
    
    for (int i = 1; i < (int) m_diveProfile.size(); i++) {
        m_diveProfile[i].updateRunTime(&m_diveProfile[i - 1]);
    }
}

// Save and load dive plan
bool DivePlan::saveDiveToFile(const std::string& filePath) {
    bool result = ErrorHandler::tryFileOperation([&]() {
        // Log performance
        QElapsedTimer timer;
        timer.start();

        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            logWrite("Failed to open file for writing: ", filePath);
            return;
        }

        // Write a version identifier for future compatibility
        uint32_t fileVersion = 1;
        file.write(reinterpret_cast<const char*>(&fileVersion), sizeof(fileVersion));

        // Save basic dive parameters
        file.write(reinterpret_cast<const char*>(&m_mode), sizeof(m_mode));
        file.write(reinterpret_cast<const char*>(&m_bailout), sizeof(m_bailout));
        file.write(reinterpret_cast<const char*>(&m_diveNumber), sizeof(m_diveNumber));
        file.write(reinterpret_cast<const char*>(&m_boosted), sizeof(m_boosted));
        file.write(reinterpret_cast<const char*>(&m_mission), sizeof(m_mission));
        file.write(reinterpret_cast<const char*>(&m_firstDecoDepth), sizeof(m_firstDecoDepth));
        
        // Save summary values
        file.write(reinterpret_cast<const char*>(&m_tts), sizeof(m_tts));
        file.write(reinterpret_cast<const char*>(&m_ttsDelta), sizeof(m_ttsDelta));
        file.write(reinterpret_cast<const char*>(&m_ap), sizeof(m_ap));
        file.write(reinterpret_cast<const char*>(&m_maxResult.first), sizeof(m_maxResult.first));
        file.write(reinterpret_cast<const char*>(&m_maxResult.second), sizeof(m_maxResult.second));
        file.write(reinterpret_cast<const char*>(&m_tp), sizeof(m_tp));
        file.write(reinterpret_cast<const char*>(&m_turnTts), sizeof(m_turnTts));

        // Save StopSteps
        size_t stopStepsCount = m_stopSteps.m_stopSteps.size();
        file.write(reinterpret_cast<const char*>(&stopStepsCount), sizeof(stopStepsCount));
        for (const auto& stopStep : m_stopSteps.m_stopSteps) {
            file.write(reinterpret_cast<const char*>(&stopStep.m_depth), sizeof(stopStep.m_depth));
            file.write(reinterpret_cast<const char*>(&stopStep.m_time), sizeof(stopStep.m_time));
        }

        // Save SetPoints
        size_t setPointsCount = m_setPoints.m_depths.size();
        file.write(reinterpret_cast<const char*>(&setPointsCount), sizeof(setPointsCount));
        for (size_t i = 0; i < setPointsCount; ++i) {
            file.write(reinterpret_cast<const char*>(&m_setPoints.m_depths[i]), sizeof(double));
            file.write(reinterpret_cast<const char*>(&m_setPoints.m_setPoints[i]), sizeof(double));
        }

        // Save available gases
        size_t gasCount = m_gasAvailable.size();
        file.write(reinterpret_cast<const char*>(&gasCount), sizeof(gasCount));
        for (const auto& gas : m_gasAvailable) {
            // Save Gas properties
            file.write(reinterpret_cast<const char*>(&gas.m_gas.m_o2Percent), sizeof(double));
            file.write(reinterpret_cast<const char*>(&gas.m_gas.m_hePercent), sizeof(double));
            file.write(reinterpret_cast<const char*>(&gas.m_gas.m_gasType), sizeof(GasType));
            file.write(reinterpret_cast<const char*>(&gas.m_gas.m_gasStatus), sizeof(GasStatus));
            
            // Save GasAvailable properties
            file.write(reinterpret_cast<const char*>(&gas.m_switchDepth), sizeof(double));
            file.write(reinterpret_cast<const char*>(&gas.m_switchPpO2), sizeof(double));
            file.write(reinterpret_cast<const char*>(&gas.m_nbTanks), sizeof(int));
            file.write(reinterpret_cast<const char*>(&gas.m_tankCapacity), sizeof(double));
            file.write(reinterpret_cast<const char*>(&gas.m_fillingPressure), sizeof(double));
            file.write(reinterpret_cast<const char*>(&gas.m_reservePressure), sizeof(double));
            file.write(reinterpret_cast<const char*>(&gas.m_consumption), sizeof(double));
            file.write(reinterpret_cast<const char*>(&gas.m_endPressure), sizeof(double));
        }

        // Save initial pressure
        size_t initialPressureCount = m_initialPressure.size();
        file.write(reinterpret_cast<const char*>(&initialPressureCount), sizeof(initialPressureCount));
        for (const auto& pressure : m_initialPressure) {
            file.write(reinterpret_cast<const char*>(&pressure.m_pN2), sizeof(double));
            file.write(reinterpret_cast<const char*>(&pressure.m_pHe), sizeof(double));
            file.write(reinterpret_cast<const char*>(&pressure.m_pInert), sizeof(double));
        }

        // Save GF values (that might have been modified in the summary widget)
        file.write(reinterpret_cast<const char*>(&g_parameters.m_gf), sizeof(g_parameters.m_gf));

        // Save dive profile
        size_t profileCount = m_diveProfile.size();
        file.write(reinterpret_cast<const char*>(&profileCount), sizeof(profileCount));
        for (const auto& step : m_diveProfile) {
            // Basic step properties
            file.write(reinterpret_cast<const char*>(&step.m_phase), sizeof(Phase));
            file.write(reinterpret_cast<const char*>(&step.m_mode), sizeof(stepMode));
            file.write(reinterpret_cast<const char*>(&step.m_startDepth), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_endDepth), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_time), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_runTime), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_pAmbStartDepth), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_pAmbEndDepth), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_pAmbMax), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_pO2Max), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_o2Percent), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_n2Percent), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_hePercent), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_gf), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_gfSurface), sizeof(double));
            
            // Save compartment data for each step
            for (const auto& pp : step.m_ppMax) {
                file.write(reinterpret_cast<const char*>(&pp.m_pN2), sizeof(double));
                file.write(reinterpret_cast<const char*>(&pp.m_pHe), sizeof(double));
                file.write(reinterpret_cast<const char*>(&pp.m_pInert), sizeof(double));
            }
            
            for (const auto& pp : step.m_ppMaxAdjustedGF) {
                file.write(reinterpret_cast<const char*>(&pp.m_pN2), sizeof(double));
                file.write(reinterpret_cast<const char*>(&pp.m_pHe), sizeof(double));
                file.write(reinterpret_cast<const char*>(&pp.m_pInert), sizeof(double));
            }
            
            for (const auto& pp : step.m_ppActual) {
                file.write(reinterpret_cast<const char*>(&pp.m_pN2), sizeof(double));
                file.write(reinterpret_cast<const char*>(&pp.m_pHe), sizeof(double));
                file.write(reinterpret_cast<const char*>(&pp.m_pInert), sizeof(double));
            }
            
            // Consumption and other metrics
            file.write(reinterpret_cast<const char*>(&step.m_sacRate), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_ambConsumptionAtDepth), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_stepConsumption), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_gasDensity), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_endWithoutO2), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_endWithO2), sizeof(double));
            
            // Oxygen toxicity metrics
            file.write(reinterpret_cast<const char*>(&step.m_cnsMaxMinSingleDive), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_cnsStepSingleDive), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_cnsTotalSingleDive), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_cnsMaxMinMultipleDives), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_cnsStepMultipleDives), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_cnsTotalMultipleDives), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_otuPerMin), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_otuStep), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_otuTotal), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_ceiling), sizeof(double));
        }

        // Save time profile
        size_t timeProfileCount = m_timeProfile.size();
        file.write(reinterpret_cast<const char*>(&timeProfileCount), sizeof(timeProfileCount));
        for (const auto& step : m_timeProfile) {
            // Basic step properties
            file.write(reinterpret_cast<const char*>(&step.m_phase), sizeof(Phase));
            file.write(reinterpret_cast<const char*>(&step.m_mode), sizeof(stepMode));
            file.write(reinterpret_cast<const char*>(&step.m_startDepth), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_endDepth), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_time), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_runTime), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_pAmbStartDepth), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_pAmbEndDepth), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_pAmbMax), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_pO2Max), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_o2Percent), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_n2Percent), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_hePercent), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_gf), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_gfSurface), sizeof(double));
            
            // Save compartment data for each step
            for (const auto& pp : step.m_ppMax) {
                file.write(reinterpret_cast<const char*>(&pp.m_pN2), sizeof(double));
                file.write(reinterpret_cast<const char*>(&pp.m_pHe), sizeof(double));
                file.write(reinterpret_cast<const char*>(&pp.m_pInert), sizeof(double));
            }
            
            for (const auto& pp : step.m_ppMaxAdjustedGF) {
                file.write(reinterpret_cast<const char*>(&pp.m_pN2), sizeof(double));
                file.write(reinterpret_cast<const char*>(&pp.m_pHe), sizeof(double));
                file.write(reinterpret_cast<const char*>(&pp.m_pInert), sizeof(double));
            }
            
            for (const auto& pp : step.m_ppActual) {
                file.write(reinterpret_cast<const char*>(&pp.m_pN2), sizeof(double));
                file.write(reinterpret_cast<const char*>(&pp.m_pHe), sizeof(double));
                file.write(reinterpret_cast<const char*>(&pp.m_pInert), sizeof(double));
            }
            
            // Consumption and other metrics
            file.write(reinterpret_cast<const char*>(&step.m_sacRate), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_ambConsumptionAtDepth), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_stepConsumption), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_gasDensity), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_endWithoutO2), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_endWithO2), sizeof(double));
            
            // Oxygen toxicity metrics
            file.write(reinterpret_cast<const char*>(&step.m_cnsMaxMinSingleDive), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_cnsStepSingleDive), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_cnsTotalSingleDive), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_cnsMaxMinMultipleDives), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_cnsStepMultipleDives), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_cnsTotalMultipleDives), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_otuPerMin), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_otuStep), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_otuTotal), sizeof(double));
            file.write(reinterpret_cast<const char*>(&step.m_ceiling), sizeof(double));
        }

        file.close();
        logWrite("Dive plan saved successfully in ", timer.elapsed(), " ms to ", filePath);
    }, filePath, "Error Saving Dive Plan");

    if (result) {
        m_filePath = filePath;
    }
    
    return result;
}

std::unique_ptr<DivePlan> DivePlan::loadDiveFromFile(const std::string& filePath) {
    std::unique_ptr<DivePlan> loadedPlan = nullptr;
    
    bool success = ErrorHandler::tryFileOperation([&]() {
        // Log performance
        QElapsedTimer timer;
        timer.start();

        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            logWrite("Failed to open file for reading: ", filePath);
            return;
        }

        // Read version identifier
        uint32_t fileVersion;
        file.read(reinterpret_cast<char*>(&fileVersion), sizeof(fileVersion));

        if (fileVersion != 1) {
            logWrite("Unsupported file version: ", fileVersion);
            return;
        }

        // Read basic parameters
        diveMode mode;
        bool bailout;
        int diveNumber;
        bool boosted;
        double mission;
        double firstDecoDepth;
        
        file.read(reinterpret_cast<char*>(&mode), sizeof(mode));
        file.read(reinterpret_cast<char*>(&bailout), sizeof(bailout));
        file.read(reinterpret_cast<char*>(&diveNumber), sizeof(diveNumber));
        file.read(reinterpret_cast<char*>(&boosted), sizeof(boosted));
        file.read(reinterpret_cast<char*>(&mission), sizeof(mission));
        file.read(reinterpret_cast<char*>(&firstDecoDepth), sizeof(firstDecoDepth));

        // Read summary values (we'll set these after loading everything else)
        double tts, ttsDelta, ap, maxTimeResult, maxTTSResult, tp, turnTts;
        file.read(reinterpret_cast<char*>(&tts), sizeof(tts));
        file.read(reinterpret_cast<char*>(&ttsDelta), sizeof(ttsDelta));
        file.read(reinterpret_cast<char*>(&ap), sizeof(ap));
        file.read(reinterpret_cast<char*>(&maxTimeResult), sizeof(maxTimeResult));
        file.read(reinterpret_cast<char*>(&maxTTSResult), sizeof(maxTTSResult));
        file.read(reinterpret_cast<char*>(&tp), sizeof(tp));
        file.read(reinterpret_cast<char*>(&turnTts), sizeof(turnTts));

        // Read StopSteps
        StopSteps stopSteps;
        size_t stopStepsCount;
        file.read(reinterpret_cast<char*>(&stopStepsCount), sizeof(stopStepsCount));
        for (size_t i = 0; i < stopStepsCount; ++i) {
            double depth, time;
            file.read(reinterpret_cast<char*>(&depth), sizeof(depth));
            file.read(reinterpret_cast<char*>(&time), sizeof(time));
            stopSteps.addStopStep(depth, time);
        }

        // Read SetPoints
        SetPoints setPoints;
        size_t setPointsCount;
        file.read(reinterpret_cast<char*>(&setPointsCount), sizeof(setPointsCount));
        setPoints.m_depths.clear();
        setPoints.m_setPoints.clear();
        for (size_t i = 0; i < setPointsCount; ++i) {
            double depth, setPoint;
            file.read(reinterpret_cast<char*>(&depth), sizeof(depth));
            file.read(reinterpret_cast<char*>(&setPoint), sizeof(setPoint));
            setPoints.m_depths.push_back(depth);
            setPoints.m_setPoints.push_back(setPoint);
        }
        setPoints.sortSetPoints();

        // Read available gases
        std::vector<GasAvailable> gasAvailable;
        size_t gasCount;
        file.read(reinterpret_cast<char*>(&gasCount), sizeof(gasCount));
        for (size_t i = 0; i < gasCount; ++i) {
            double o2Percent, hePercent;
            GasType gasType;
            GasStatus gasStatus;
            
            // Read Gas properties
            file.read(reinterpret_cast<char*>(&o2Percent), sizeof(double));
            file.read(reinterpret_cast<char*>(&hePercent), sizeof(double));
            file.read(reinterpret_cast<char*>(&gasType), sizeof(GasType));
            file.read(reinterpret_cast<char*>(&gasStatus), sizeof(GasStatus));
            
            // Create Gas object and GasAvailable object
            Gas gas(o2Percent, hePercent, gasType, gasStatus);
            GasAvailable gasObj(gas);
            
            // Read GasAvailable properties
            file.read(reinterpret_cast<char*>(&gasObj.m_switchDepth), sizeof(double));
            file.read(reinterpret_cast<char*>(&gasObj.m_switchPpO2), sizeof(double));
            file.read(reinterpret_cast<char*>(&gasObj.m_nbTanks), sizeof(int));
            file.read(reinterpret_cast<char*>(&gasObj.m_tankCapacity), sizeof(double));
            file.read(reinterpret_cast<char*>(&gasObj.m_fillingPressure), sizeof(double));
            file.read(reinterpret_cast<char*>(&gasObj.m_reservePressure), sizeof(double));
            file.read(reinterpret_cast<char*>(&gasObj.m_consumption), sizeof(double));
            file.read(reinterpret_cast<char*>(&gasObj.m_endPressure), sizeof(double));
            
            gasAvailable.push_back(gasObj);
        }

        // Read initial pressure
        std::vector<CompartmentPP> initialPressure;
        size_t initialPressureCount;
        file.read(reinterpret_cast<char*>(&initialPressureCount), sizeof(initialPressureCount));
        for (size_t i = 0; i < initialPressureCount; ++i) {
            double pN2, pHe, pInert;
            file.read(reinterpret_cast<char*>(&pN2), sizeof(double));
            file.read(reinterpret_cast<char*>(&pHe), sizeof(double));
            file.read(reinterpret_cast<char*>(&pInert), sizeof(double));
            initialPressure.push_back(CompartmentPP(pN2, pHe, pInert));
        }

        // Read saved GF values
        double savedGF[2];
        file.read(reinterpret_cast<char*>(&savedGF), sizeof(savedGF));
        g_parameters.m_gf[0] = savedGF[0];
        g_parameters.m_gf[1] = savedGF[1];

        // Now we have enough information to create a new DivePlan object
        // Get the first stop step's depth and time for the constructor
        double depth = 0.0, time = 0.0;
        if (!stopSteps.m_stopSteps.empty()) {
            depth = stopSteps.m_stopSteps[0].m_depth;
            time = stopSteps.m_stopSteps[0].m_time;
        }
        
        loadedPlan = std::make_unique<DivePlan>(depth, time, mode, diveNumber, initialPressure);
        loadedPlan->m_bailout = bailout;
        loadedPlan->m_boosted = boosted;
        loadedPlan->m_mission = mission;
        loadedPlan->m_firstDecoDepth = firstDecoDepth;
        
        // Set summary values
        loadedPlan->m_tts = tts;
        loadedPlan->m_ttsDelta = ttsDelta;
        loadedPlan->m_ap = ap;
        loadedPlan->m_maxResult = std::make_pair(maxTimeResult, maxTTSResult);
        loadedPlan->m_tp = tp;
        loadedPlan->m_turnTts = turnTts;
        
        // Replace the automatically created stop steps with our loaded ones
        loadedPlan->m_stopSteps = stopSteps;
        
        // Replace the setpoints with our loaded ones
        loadedPlan->m_setPoints = setPoints;
        
        // Replace the gas list with our loaded one
        loadedPlan->m_gasAvailable = gasAvailable;
        
        // Replace the initial pressure with our loaded one
        loadedPlan->m_initialPressure = initialPressure;
        
        // Clear the automatically created dive profile
        loadedPlan->m_diveProfile.clear();
        
        // Read dive profile
        size_t profileCount;
        file.read(reinterpret_cast<char*>(&profileCount), sizeof(profileCount));
        for (size_t i = 0; i < profileCount; ++i) {
            DiveStep step;
            
            // Read basic step properties
            file.read(reinterpret_cast<char*>(&step.m_phase), sizeof(Phase));
            file.read(reinterpret_cast<char*>(&step.m_mode), sizeof(stepMode));
            file.read(reinterpret_cast<char*>(&step.m_startDepth), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_endDepth), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_time), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_runTime), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_pAmbStartDepth), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_pAmbEndDepth), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_pAmbMax), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_pO2Max), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_o2Percent), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_n2Percent), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_hePercent), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_gf), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_gfSurface), sizeof(double));
            
            // Read compartment data
            for (auto& pp : step.m_ppMax) {
                file.read(reinterpret_cast<char*>(&pp.m_pN2), sizeof(double));
                file.read(reinterpret_cast<char*>(&pp.m_pHe), sizeof(double));
                file.read(reinterpret_cast<char*>(&pp.m_pInert), sizeof(double));
            }
            
            for (auto& pp : step.m_ppMaxAdjustedGF) {
                file.read(reinterpret_cast<char*>(&pp.m_pN2), sizeof(double));
                file.read(reinterpret_cast<char*>(&pp.m_pHe), sizeof(double));
                file.read(reinterpret_cast<char*>(&pp.m_pInert), sizeof(double));
            }
            
            for (auto& pp : step.m_ppActual) {
                file.read(reinterpret_cast<char*>(&pp.m_pN2), sizeof(double));
                file.read(reinterpret_cast<char*>(&pp.m_pHe), sizeof(double));
                file.read(reinterpret_cast<char*>(&pp.m_pInert), sizeof(double));
            }
            
            // Read consumption and other metrics
            file.read(reinterpret_cast<char*>(&step.m_sacRate), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_ambConsumptionAtDepth), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_stepConsumption), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_gasDensity), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_endWithoutO2), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_endWithO2), sizeof(double));
            
            // Read oxygen toxicity metrics
            file.read(reinterpret_cast<char*>(&step.m_cnsMaxMinSingleDive), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_cnsStepSingleDive), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_cnsTotalSingleDive), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_cnsMaxMinMultipleDives), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_cnsStepMultipleDives), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_cnsTotalMultipleDives), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_otuPerMin), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_otuStep), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_otuTotal), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_ceiling), sizeof(double));
            
            loadedPlan->m_diveProfile.push_back(step);
        }

        // Read time profile
        loadedPlan->m_timeProfile.clear();
        size_t timeProfileCount;
        file.read(reinterpret_cast<char*>(&timeProfileCount), sizeof(timeProfileCount));
        for (size_t i = 0; i < timeProfileCount; ++i) {
            DiveStep step;
            
            // Read basic step properties
            file.read(reinterpret_cast<char*>(&step.m_phase), sizeof(Phase));
            file.read(reinterpret_cast<char*>(&step.m_mode), sizeof(stepMode));
            file.read(reinterpret_cast<char*>(&step.m_startDepth), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_endDepth), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_time), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_runTime), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_pAmbStartDepth), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_pAmbEndDepth), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_pAmbMax), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_pO2Max), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_o2Percent), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_n2Percent), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_hePercent), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_gf), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_gfSurface), sizeof(double));
            
            // Read compartment data
            for (auto& pp : step.m_ppMax) {
                file.read(reinterpret_cast<char*>(&pp.m_pN2), sizeof(double));
                file.read(reinterpret_cast<char*>(&pp.m_pHe), sizeof(double));
                file.read(reinterpret_cast<char*>(&pp.m_pInert), sizeof(double));
            }
            
            for (auto& pp : step.m_ppMaxAdjustedGF) {
                file.read(reinterpret_cast<char*>(&pp.m_pN2), sizeof(double));
                file.read(reinterpret_cast<char*>(&pp.m_pHe), sizeof(double));
                file.read(reinterpret_cast<char*>(&pp.m_pInert), sizeof(double));
            }
            
            for (auto& pp : step.m_ppActual) {
                file.read(reinterpret_cast<char*>(&pp.m_pN2), sizeof(double));
                file.read(reinterpret_cast<char*>(&pp.m_pHe), sizeof(double));
                file.read(reinterpret_cast<char*>(&pp.m_pInert), sizeof(double));
            }
            
            // Read consumption and other metrics
            file.read(reinterpret_cast<char*>(&step.m_sacRate), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_ambConsumptionAtDepth), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_stepConsumption), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_gasDensity), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_endWithoutO2), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_endWithO2), sizeof(double));
            
            // Read oxygen toxicity metrics
            file.read(reinterpret_cast<char*>(&step.m_cnsMaxMinSingleDive), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_cnsStepSingleDive), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_cnsTotalSingleDive), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_cnsMaxMinMultipleDives), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_cnsStepMultipleDives), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_cnsTotalMultipleDives), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_otuPerMin), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_otuStep), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_otuTotal), sizeof(double));
            file.read(reinterpret_cast<char*>(&step.m_ceiling), sizeof(double));
            
            loadedPlan->m_timeProfile.push_back(step);
        }

        file.close();
        logWrite("Dive plan loaded successfully in ", timer.elapsed(), " ms from ", filePath);
        
    }, filePath, "Error Loading Dive Plan");

    if (success) {
        loadedPlan->setFilePath(filePath);
    }

    return loadedPlan;
}

} // namespace DiveComputer
