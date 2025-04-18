#ifndef DIVE_PLAN_HPP
#define DIVE_PLAN_HPP

#include <vector>
#include <memory>
#include <set>

#include "log_info.hpp"
#include "enum.hpp"
#include "dive_step.hpp"
#include "stop_steps.hpp"
#include "compartments.hpp"
#include "parameters.hpp"
#include "gas.hpp"
#include "gaslist.hpp"
#include "set_points.hpp"
#include "oxygen_toxicity.hpp"

namespace DiveComputer {

// Create a new struct for gas tracking
struct GasAvailable {
    Gas    m_gas;
    double m_switchDepth;
    double m_switchPpO2;
    int    m_nbTanks;
    double m_tankCapacity;    // in liters
    double m_fillingPressure; // in bar
    double m_reservePressure; // in bar
    double m_consumption;     // accumulated during dive
    double m_endPressure;     // calculated at end of dive
    
    GasAvailable(const Gas& g) : m_gas(g), m_nbTanks(1), m_tankCapacity(11.0), 
                                m_fillingPressure(200.0), m_reservePressure(70.0),
                                m_consumption(0.0), m_endPressure(200.0) {}
};

// Dive profile management class
class DivePlan {
public:
    DivePlan(double depth, double time, diveMode mode, int diveNumber, std::vector<CompartmentPP> initialPressure);
    ~DivePlan() = default;

    StopSteps m_stopSteps;
    diveMode  m_mode;

    // Dive parameters
    bool m_bailout = false;
    int  m_diveNumber = 0;
    bool m_boosted = true;
    SetPoints m_setPoints;
    double m_mission = 0.0;

    // Summary variables
    double m_tts = 0;
    double m_ttsDelta = 0;
    double m_ap = 0;
    std::pair<double, double> m_maxResult;
    double m_tp = 0;
    double m_turnTts = 0;

    // Dive variables
    std::vector<DiveStep> m_diveProfile;
    std::vector<DiveStep> m_timeProfile;
    std::vector<GasAvailable> m_gasAvailable;
    std::vector<CompartmentPP> m_initialPressure;

    // Core methods
    int  nbOfSteps();
    void loadAvailableGases();
    void buildDivePlan();
    void calculateDivePlan(bool printLog = true);
    void calculateDiveSummary(bool printLog = true);
    void calculateGasConsumption(bool printLog = true);
    void calculateOtherVariables(double GF, bool printLog = true);
    void calculateTimeProfile(bool printLog = true);

    // Action methods
    std::pair<double, double> getMaxTimeAndTTS();
    void   optimiseDecoGas();
    double getTTS();
    double getTTSDelta(double incrementTime);
    double getAP();
    double getTP();
    double getTurnTTS();
    double getNoFlyTime();

    // Print-to-terminal functions
    void printPlan(std::vector<DiveStep> profile);
    void printCompartmentDetails(int compartment);
    void printGF();
    void printO2Exposure();
    void printSummary();

    // Save and load dive plan
    bool saveDiveToFile(const std::string& filePath);
    static std::unique_ptr<DivePlan> loadDiveFromFile(const std::string& filePath);
    std::string getFilePath() const { return m_filePath; }
    void setFilePath(const std::string& path) { m_filePath = path; }

private:
    double m_firstDecoDepth;
    std::string m_filePath;  // Store the file path for reloading

    // Helper methods
    void   clear();
    void   clearDecoSteps();
    void   sortGases();
    void   applyGases();
    void   calculatePPInertGas();
    void   calculatePPInertGasMax();
    void   applyGF();
    void   setFirstDecoDepth();
    void   calculateDecoSteps();
    bool   getIfBreachingDecoLimitsInRange(int deco, int next_deco);
    void   calculatePPInertGasInRange(int deco, int next_deco);
    double calculateFirstStopDepth(double maxDepth);
    void   processAscentStops(const std::vector<double>& ascentStops);
    bool   enoughGasAvailable();

    DiveStep& addStep(double start_depth, double end_depth, double time, Phase phase, stepMode mode);
    DiveStep& insertStep(int index, double start_depth, double end_depth, double time, Phase phase, stepMode mode);
    void deleteStep(int index);

    // update variable functions
    void updateStepsPhaseFromFirstDeco();
    void updatePpAmb();
    void updateCeiling(double GF);
    void updateOxygenToxicity();
    void updateConsumptions();
    void updateGFSurface();
    void updateRunTimes();
    void updateVariables(double GF);
};

} // namespace DiveComputer

#endif // DIVE_PLAN_HPP
