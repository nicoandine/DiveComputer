#ifndef DIVE_PLAN_GUI_COMPARTMENT_GRAPH_HPP
#define DIVE_PLAN_GUI_COMPARTMENT_GRAPH_HPP

#include "qtheaders.hpp"
#include "dive_plan.hpp"
#include "compartments.hpp"
#include "constants.hpp"
#include "ui_utils.hpp"
#include "qcustomplot.hpp"

namespace DiveComputer {

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
    QComboBox* m_compartmentSelector;
    QCustomPlot* m_graphWidget;
    
    // Styling constants
    static constexpr int WindowWidth = 800;
    static constexpr int WindowHeight = 600;
    
    // Setup methods
    void setupUI();
    void setupGraph();
    void updateGraph(int compartmentIndex);
    
private slots:
    void onCompartmentChanged(int index);
};

} // namespace DiveComputer

#endif // DIVE_PLAN_GUI_COMPARTMENT_GRAPH_HPP