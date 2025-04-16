#ifndef DIVE_PLAN_GUI_COMPARTMENT_GRAPH_HPP
#define DIVE_PLAN_GUI_COMPARTMENT_GRAPH_HPP

#include "qtheaders.hpp"
#include "dive_plan.hpp"
#include "compartments.hpp"
#include "constants.hpp"
#include "ui_utils.hpp"
#include "qcustomplot.hpp"

namespace DiveComputer {

// Graph mode enum
enum class GraphMode {
    PRESSURE,
    TIME
};

// Gas type enum
enum class GraphGasType {
    INERT,
    N2,
    HE
};

class CompartmentGraphWindow : public QMainWindow {
    Q_OBJECT
    
public:
    CompartmentGraphWindow(const DivePlan* divePlan, QWidget *parent = nullptr);
    ~CompartmentGraphWindow() override = default;

protected:
    void resizeEvent(QResizeEvent* event) override;
    
private:
    // Reference to the dive plan
    const DivePlan* m_divePlan;
    
    // UI Elements
    QComboBox* m_graphGasTypeSelector;
    QComboBox* m_graphModeSelector;
    QComboBox* m_compartmentSelector;
    QCustomPlot* m_graphWidget;
    
    // Current graph mode and gas type
    GraphMode m_graphMode = GraphMode::PRESSURE;
    GraphGasType m_graphGasType = GraphGasType::INERT;
    
    // Styling constants
    static constexpr int WindowWidth = 800;
    static constexpr int WindowHeight = 600;
    
    // Setup methods
    void setupUI();
    void setupGraph();
    void updateGraph(int compartmentIndex);
    
    // Helper function to get current gas pressure
    double getGasPressure(const CompartmentPP& pp) const;
    double getAmbientGasPressure(double pressure, const DiveStep& step) const;
    QString returnQStringGasType(GraphGasType type);

private slots:
    void onCompartmentChanged(int index);
    void onGraphModeChanged(int index);
    void onGasTypeChanged(int index);
};

} // namespace DiveComputer

#endif // DIVE_PLAN_GUI_COMPARTMENT_GRAPH_HPP