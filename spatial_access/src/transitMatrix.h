#pragma once

#include "threadUtilities.h"
#include "dataFrame.h"
#include "Graph.h"
#include "MinHeap.h"
#include "userDataContainer.h"

#include <mutex>


/*write_row: write a row to file*/
void calculateRow(const std::vector<int> &dist, graphWorkerArgs *wa, int src);


/* Calulates distances of shortest paths from src to all*/
/* vertices. It is a O(ELogV) function*/
void dijkstra(int src, graphWorkerArgs *wa);

void graphWorkerHandler(graphWorkerArgs* wa);

void rangeWorkerHandler(rangeWorkerArgs* wa);

void calculateValuesForOneIndex(unsigned long int index, rangeWorkerArgs *wa);


namespace lmnoel {
class transitMatrix {
public:
    typedef unsigned long int label;
    typedef unsigned short int value;
    dataFrame df;
    userDataContainer userSourceDataContainer;
    userDataContainer userDestDataContainer;
    bool isSymmetric;
    Graph graph;
    int numNodes;
    void addToUserSourceDataContainer(int networkNodeId, unsigned long int id, int lastMileDistance, bool isBidirectional);
    void addToUserDestDataContainer(int networkNodeId, unsigned long int id, int lastMileDistance);
    void addEdgeToGraph(int src, int dest, int weight, bool isBidirectional);
    transitMatrix(int V, bool isSymmetric);
    transitMatrix(const std::string& infile, bool isSymmetric, bool isOTPMatrix);
    void compute(int numThreads);
    transitMatrix(void);
    int get(unsigned long int source, unsigned long intdest) const;
    const std::vector<std::pair<unsigned long int, unsigned short int>> getValuesBySource(unsigned long int source_id, bool sort);
    const std::vector<std::pair<unsigned long int, unsigned short int>> getValuesByDest(unsigned long int dest_id, bool sort);

    void loadFromDisk(void);
    void prepareDataFrame();
    bool writeCSV(const std::string &outfile);
    bool writeTMX(const std::string &outfile);
    void printDataFrame() const;
    void calculateSourcesInRange(unsigned int threshold, int numThreads);
    void calculateDestsInRange(unsigned int threshold, int numThreads);
    const std::unordered_map<unsigned long int, std::vector<unsigned long int>>& getDestsInRange(unsigned int range, int numThreads);
    const std::unordered_map<unsigned long int, std::vector<unsigned long int>>& getSourcesInRange(unsigned int range, int numThreads);
private:
    std::unordered_map<unsigned long int, std::vector<unsigned long int>> sourcesInRange;
    std::unordered_map<unsigned long int, std::vector<unsigned long int>> destsInRange;
    unsigned int sourcesInRangeThreshold;
    unsigned int destsInRangeThreshold;

};

} // namespace lnoel