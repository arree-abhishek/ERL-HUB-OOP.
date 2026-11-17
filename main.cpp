#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <memory>
#include <numeric>
#include <limits>
#include <cstdio>

namespace ReliefOps {
namespace Logistics {
    inline double calculateDistance(double speedKmph, double hours) { return speedKmph * hours; }
    inline double calculatePayload(double weightKg, double quantity) { return weightKg * quantity; }

    double estimatePriority(double urgency, double distanceKm) {
        return urgency / (distanceKm + 1.0);
    }
    double estimatePriority(double urgency, double distanceKm, double medicalFactor) {
        return (urgency * medicalFactor) / (distanceKm + 1.0);
    }
    int allocateSupplies(int requestedUnits, int availableUnits) {
        return std::min(requestedUnits, availableUnits);
    }
    int allocateSupplies(int requestedUnits, int availableUnits, int emergencyPriority) {
        return emergencyPriority >= 8 ? std::min(requestedUnits, availableUnits) : std::min(requestedUnits, availableUnits / 2);
    }
}
namespace Inventory {
    inline double remainingStock(double stock, double dispatched) { return stock - dispatched; }
}
}

class IncidentCommander;

class ReliefCargo {
    std::string name;
    double unitWeightKg;
    int quantity;
public:
    ReliefCargo(std::string n = "General Supplies", double w = 1.0, int q = 0)
        : name(std::move(n)), unitWeightKg(w), quantity(q) {
        if (unitWeightKg < 0 || quantity < 0) throw std::invalid_argument("Cargo values cannot be negative");
    }
    ReliefCargo(const ReliefCargo& other)
        : name(other.name), unitWeightKg(other.unitWeightKg), quantity(other.quantity) {}
    const std::string& getName() const { return name; }
    double totalWeight() const { return unitWeightKg * quantity; }
    int getQuantity() const { return quantity; }
    void add(int amount) { if (amount < 0) throw std::invalid_argument("Negative restock"); quantity += amount; }
    void remove(int amount) { if (amount < 0 || amount > quantity) throw std::out_of_range("Insufficient cargo stock"); quantity -= amount; }
    ReliefCargo& operator+=(int amount) { add(amount); return *this; }
    ReliefCargo& operator-=(int amount) { remove(amount); return *this; }
    bool operator==(const ReliefCargo& rhs) const { return name == rhs.name; }
    friend std::ostream& operator<<(std::ostream& os, const ReliefCargo& c) {
        return os << c.name << " | quantity: " << c.quantity << " | total weight: " << c.totalWeight() << " kg";
    }
    friend std::istream& operator>>(std::istream& is, ReliefCargo& c) {
        return is >> c.name >> c.unitWeightKg >> c.quantity;
    }
    friend class IncidentCommander;
};

class IncidentCommander {
public:
    void emergencyRestock(ReliefCargo& cargo, int amount) {
        // Friend access is used for a controlled emergency inventory override.
        if (amount > 0) cargo.quantity += amount;
    }
};

class DistributionDepot {
    std::string depotName;
    std::unordered_map<std::string, int> stock;
public:
    explicit DistributionDepot(std::string name) : depotName(std::move(name)) {}
    void addCargo(const ReliefCargo& cargo) { stock[cargo.getName()] += cargo.getQuantity(); }
    bool dispatch(const std::string& item, int quantity) {
        auto it = stock.find(item);
        if (it == stock.end() || quantity < 0 || it->second < quantity) return false;
        it->second -= quantity;
        return true;
    }
    DistributionDepot& operator+=(const ReliefCargo& cargo) { addCargo(cargo); return *this; }
    DistributionDepot& operator-=(const ReliefCargo& cargo) {
        if (!dispatch(cargo.getName(), cargo.getQuantity()))
            throw std::out_of_range("Insufficient depot stock for manifest dispatch");
        return *this;
    }
    void print() const {
        std::cout << "\nDepot: " << depotName << "\n";
        for (const auto& entry : stock) std::cout << "  " << entry.first << ": " << entry.second << "\n";
    }
};

class CargoOverweightException : public std::runtime_error { public: explicit CargoOverweightException(const std::string& s) : std::runtime_error(s) {} };
class FuelDepletionException : public std::runtime_error { public: explicit FuelDepletionException(const std::string& s) : std::runtime_error(s) {} };
class DamagedCorridorException : public std::runtime_error { public: explicit DamagedCorridorException(const std::string& s) : std::runtime_error(s) {} };

struct GPSBeacon { std::string coordinate; explicit GPSBeacon(std::string c = "28.6139,77.2090") : coordinate(std::move(c)) {} };
struct EmergencyLifeSupportUnit { bool active = true; int oxygenHours = 12; };

class ReliefVehicle {
protected:
    std::string vehicleId;
    double capacityKg;
    double fuelLitres;
    static int operationalVehicles;
    static double totalDispatchedKg;
public:
    ReliefVehicle(std::string id, double capacity, double fuel)
        : vehicleId(std::move(id)), capacityKg(capacity), fuelLitres(fuel) { ++operationalVehicles; }
    virtual ~ReliefVehicle() { --operationalVehicles; }
    virtual std::string vehicleType() const = 0;
    virtual double routeTime(double distanceKm) const = 0;
    virtual void dispatch(double cargoKg, double distanceKm, bool corridorSafe) {
        if (cargoKg > capacityKg) throw CargoOverweightException("Cargo exceeds vehicle capacity");
        if (fuelLitres <= 0) throw FuelDepletionException("Vehicle has no fuel remaining");
        if (!corridorSafe) throw DamagedCorridorException("Selected route is reported damaged");
        const double requiredFuel = distanceKm * 0.15;
        if (fuelLitres < requiredFuel) throw FuelDepletionException("Insufficient fuel for this route");
        fuelLitres -= requiredFuel;
        totalDispatchedKg += cargoKg;
        std::cout << vehicleType() << " " << vehicleId << " dispatched " << cargoKg
                  << " kg over " << distanceKm << " km. ETA: " << routeTime(distanceKm) << " hours.\n";
    }
    static int getOperationalVehicles() { return operationalVehicles; }
    static double getTotalDispatchedKg() { return totalDispatchedKg; }
    const std::string& getId() const { return vehicleId; }
};
int ReliefVehicle::operationalVehicles = 0;
double ReliefVehicle::totalDispatchedKg = 0.0;

class AerialTransport : virtual public ReliefVehicle {
public:
    AerialTransport(std::string id = "AIR-01", double cap = 1200, double fuel = 500)
        : ReliefVehicle(std::move(id), cap, fuel) {}
    std::string vehicleType() const override { return "Aerial transport"; }
    double routeTime(double d) const override { return d / 180.0; }
};
class GroundConvoy : virtual public ReliefVehicle {
public:
    GroundConvoy(std::string id = "GRD-01", double cap = 5000, double fuel = 300)
        : ReliefVehicle(std::move(id), cap, fuel) {}
    std::string vehicleType() const override { return "Ground convoy"; }
    double routeTime(double d) const override { return d / 55.0; }
};
class AmphibiousCraft : public AerialTransport, public GroundConvoy {
    GPSBeacon gps;
    EmergencyLifeSupportUnit lifeSupport;
public:
    AmphibiousCraft(std::string id, double cap, double fuel, std::string location)
        : ReliefVehicle(id, cap, fuel), AerialTransport(id, cap, fuel), GroundConvoy(id, cap, fuel), gps(std::move(location)) {}
    std::string vehicleType() const override { return "Amphibious medical craft"; }
    double routeTime(double d) const override { return d / 75.0; }
    void showEquipment() const {
        std::cout << "  GPS: " << gps.coordinate << " | Life support: "
                  << (lifeSupport.active ? "active" : "inactive") << " (" << lifeSupport.oxygenHours << " oxygen hours)\n";
    }
};

// Dynamic allocation, deep copy, and destructor demonstration.
class TransportManifest {
    int* itemCounts;
    std::size_t size;
public:
    explicit TransportManifest(std::size_t n = 0) : itemCounts(n ? new int[n]() : nullptr), size(n) {}
    TransportManifest(const TransportManifest& other) : itemCounts(other.size ? new int[other.size] : nullptr), size(other.size) {
        for (std::size_t i = 0; i < size; ++i) itemCounts[i] = other.itemCounts[i];
    }
    TransportManifest& operator=(const TransportManifest& other) {
        if (this != &other) {
            int* fresh = other.size ? new int[other.size] : nullptr;
            for (std::size_t i = 0; i < other.size; ++i) fresh[i] = other.itemCounts[i];
            delete[] itemCounts; itemCounts = fresh; size = other.size;
        }
        return *this;
    }
    ~TransportManifest() { delete[] itemCounts; }
    int& operator[](std::size_t i) { if (i >= size) throw std::out_of_range("Manifest index"); return itemCounts[i]; }
    std::size_t getSize() const { return size; }
};

DistributionDepot& updateDepot(DistributionDepot& depot) { return depot; }
int& accessQuantity(ReliefCargo& cargo) {
    // Return-by-reference is illustrated through a persistent quantity reference in the demo below.
    static int quantityMirror = 0;
    quantityMirror = cargo.getQuantity();
    return quantityMirror;
}

class FuelPriority {
public:
    bool operator()(double a, double b) const { return a > b; }
};

template <typename T>
class PriorityDispatchQueue {
    std::vector<T> jobs;
public:
    void add(const T& item) { jobs.push_back(item); }
    void sortUrgent() { std::sort(jobs.begin(), jobs.end(), [](const T& a, const T& b) { return a.priority > b.priority; }); }
    void print() const {
        for (const auto& job : jobs)
            std::cout << "  " << std::left << std::setw(18) << job.name << " priority=" << job.priority << "\n";
    }
};
struct DispatchJob { std::string name; int priority; };

void saveAuditRecord(const std::string& fileName, const std::string& record) {
    // Transactional update: prepare a complete replacement file, then rename it into place.
    const std::string temporaryFile = fileName + ".tmp";
    std::ifstream existing(fileName, std::ios::binary);
    std::ofstream replacement(temporaryFile, std::ios::binary | std::ios::trunc);
    if (!replacement) throw std::runtime_error("Unable to create temporary audit file");
    if (existing) {
        replacement << existing.rdbuf();
        if (existing.bad()) throw std::runtime_error("Unable to read existing audit file");
    }
    replacement.seekp(0, std::ios::end);
    const std::size_t length = record.size();
    replacement.write(reinterpret_cast<const char*>(&length), sizeof(length));
    replacement.write(record.data(), static_cast<std::streamsize>(length));
    replacement.flush();
    if (!replacement) throw std::runtime_error("Unable to write complete audit record");
    replacement.close();
    existing.close();
    if (std::rename(temporaryFile.c_str(), fileName.c_str()) != 0) {
        std::remove(temporaryFile.c_str());
        throw std::runtime_error("Unable to commit audit file update");
    }
}
void readAuditRecords(const std::string& fileName) {
    std::ifstream in(fileName, std::ios::binary);
    if (!in) { std::cout << "No previous audit log found.\n"; return; }
    std::cout << "\nSaved audit records:\n";
    while (in) {
        std::size_t length = 0;
        in.read(reinterpret_cast<char*>(&length), sizeof(length));
        if (!in) break;
        if (length > 100000) throw std::runtime_error("Invalid audit record length");
        std::string record(length, '\0');
        in.read(&record[0], static_cast<std::streamsize>(length));
        if (!in) throw std::runtime_error("Incomplete audit record");
        std::cout << "  " << record << "\n";
    }
}

int main() {
    try {
        std::cout << "========== ERL-HUB: EMERGENCY RELIEF LOGISTICS ==========" << std::endl;
        std::cout << std::fixed << std::setprecision(2);

        // Inventory and friend-class emergency override.
        ReliefCargo water("Water", 1.0, 300);
        ReliefCargo medical("MedicalKits", 2.0, 80);
        IncidentCommander commander;
        commander.emergencyRestock(medical, 20);
        water += 10;
        water -= 10;
        std::cout << "Inventory: " << water << "\n            " << medical << "\n";

        DistributionDepot depot("North Relief Depot");
        depot += water;
        depot += medical;
        depot -= ReliefCargo("Water", 1.0, 50);
        depot.print();

        // Call by reference and return by reference.
        DistributionDepot& sameDepot = updateDepot(depot);
        (void)sameDepot;
        int& quantityRef = accessQuantity(water);
        std::cout << "Call/return reference demonstration, water quantity mirror: " << quantityRef << "\n";

        // Function overloading and inline utilities.
        std::cout << "Priority score: " << ReliefOps::Logistics::estimatePriority(9, 30) << "\n";
        std::cout << "Medical priority score: " << ReliefOps::Logistics::estimatePriority(9, 30, 2.5) << "\n";
        std::cout << "Estimated ground distance: " << ReliefOps::Logistics::calculateDistance(55, 2) << " km\n";
        std::cout << "Allocated units (priority triage): " << ReliefOps::Logistics::allocateSupplies(100, 70, 9) << "\n";

        // Dynamic memory and deep-copy constructor.
        TransportManifest manifest(3);
        manifest[0] = 40; manifest[1] = 25; manifest[2] = 10;
        TransportManifest copiedManifest = manifest;
        copiedManifest[0] = 99;
        std::cout << "Manifest deep-copy check: original first count=" << manifest[0]
                  << ", copied first count=" << copiedManifest[0] << "\n";

        // Runtime polymorphism through base pointers and hybrid/diamond inheritance.
        std::vector<std::unique_ptr<ReliefVehicle>> fleet;
        fleet.push_back(std::make_unique<AerialTransport>("AIR-11", 1200, 500));
        fleet.push_back(std::make_unique<GroundConvoy>("GRD-22", 5000, 300));
        fleet.push_back(std::make_unique<AmphibiousCraft>("AMP-33", 1800, 400, "28.61N,77.20E"));
        for (const auto& vehicle : fleet) {
            vehicle->dispatch(500, 60, true);
            if (auto* amphibious = dynamic_cast<AmphibiousCraft*>(vehicle.get())) amphibious->showEquipment();
        }
        std::cout << "Operational vehicles: " << ReliefVehicle::getOperationalVehicles() << "\n";
        std::cout << "Total dispatched: " << ReliefVehicle::getTotalDispatchedKg() << " kg\n";

        // Template priority queue and STL sorting.
        PriorityDispatchQueue<DispatchJob> queue;
        queue.add({"Trauma kits", 10});
        queue.add({"Drinking water", 7});
        queue.add({"Thermal blankets", 5});
        queue.sortUrgent();
        std::cout << "\nPriority dispatch queue:\n";
        queue.print();

        std::vector<double> fuelLevels{42.5, 18.0, 65.2, 30.0};
        std::sort(fuelLevels.begin(), fuelLevels.end(), FuelPriority{});
        std::cout << "Fuel levels sorted by priority: ";
        std::for_each(fuelLevels.begin(), fuelLevels.end(), [](double f) { std::cout << f << " "; });
        std::cout << "\n";

        // Exception handling demonstration.
        try {
            fleet[1]->dispatch(9000, 40, true);
        } catch (const CargoOverweightException& e) {
            std::cout << "Handled cargo exception: " << e.what() << "\n";
        }
        try {
            fleet[0]->dispatch(100, 20, false);
        } catch (const DamagedCorridorException& e) {
            std::cout << "Handled corridor exception: " << e.what() << "\n";
        }

        saveAuditRecord("data/transit_audit.bin", "AIR-11 dispatched 500 kg to relief zone");
        saveAuditRecord("data/transit_audit.bin", "GRD-22 dispatched 500 kg to relief zone");
        readAuditRecords("data/transit_audit.bin");

        std::cout << "\nProject demonstration completed successfully.\n";
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
