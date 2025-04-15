#ifndef GASLIST_HPP
#define GASLIST_HPP

#include "qtheaders.hpp"
#include "error_handler.hpp"
#include "gas.hpp"
#include <vector>
#include <fstream>
#include <filesystem>
#include <memory>

namespace DiveComputer {

class GasList {
public:
    // Constructor
    GasList();
    
    // Add a new gas to the list
    void addGas(double o2Percent, double hePercent, GasType gasType, GasStatus gasStatus);
    
    // Edit an existing gas
    void editGas(int index, double o2Percent, double hePercent, GasType gasType, GasStatus gasStatus);
    
    // Delete a gas from the list
    void deleteGas(int index);
    
    // Clear entire gas list
    void clearGaslist();
    
    // Load gas list from file
    bool loadGaslistFromFile();
    
    // Save gas list to file
    bool saveGaslistToFile();
    
    // Print gas list (for debugging)
    void print();
    
    // Getter for gases
    const std::vector<Gas>& getGases() const;

private:
    std::vector<Gas> m_gases;
};

// Declare global instance
extern GasList g_gasList;

} // namespace DiveComputer

#endif // GASLIST_HPP