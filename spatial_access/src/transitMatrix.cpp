#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>
#include <climits>

#include "transitMatrix.h"


/*write_row: write a row to file*/
template<class row_label_type, class col_label_type>
void calculateRow(const std::vector<unsigned short int> &dist, graphWorkerArgs<row_label_type, col_label_type> *wa, unsigned int src) {
    unsigned short int src_imp, dst_imp, calc_imp, fin_imp;
    //  iterate through each data point of the current source tract
    auto sourceTract = wa->userSourceData.retrieveTract(src);
    for (auto sourceDataPoint : sourceTract.retrieveDataPoints())
    {

        src_imp = sourceDataPoint.lastMileDistance;

        auto destNodeIds = wa->userDestData.retrieveUniqueNetworkNodeIds();
        // iterate through each dest tract

        std::unordered_map<unsigned int, unsigned short int> row_data;
        for (unsigned int destNodeId : destNodeIds)
        {
            auto destTract = wa->userDestData.retrieveTract(destNodeId);
            auto destPoints = destTract.retrieveDataPoints();
            for (auto destDataPoint : destPoints)
            {
                if (wa->df.getIsSymmetric())
                {
                    if (wa->df.isUnderDiagonal(sourceDataPoint.loc, destDataPoint.loc))
                    {
                        continue;
                    }
                }
                calc_imp = dist.at(destNodeId);
                if ((wa->df.getIsSymmetric()) && (destDataPoint.loc == sourceDataPoint.loc))
                {
                    fin_imp = 0;
                }
                else 
                {
                    dst_imp = destDataPoint.lastMileDistance;
                    if (calc_imp == USHRT_MAX)
                    {
                        fin_imp = USHRT_MAX;
                    }
                    else
                    {
                        fin_imp = dst_imp + src_imp + calc_imp;    
                    }

                }
                row_data.insert(std::make_pair(destDataPoint.loc, fin_imp));


            }

        }
        wa->df.setRowByRowLoc(row_data, sourceDataPoint.loc);
    }
}


/* The main function that calulates distances of shortest paths from src to all*/
/* vertices. It is a O(ELogV) function*/
template<class row_label_type, class col_label_type>
void dijkstra(unsigned int src, graphWorkerArgs<row_label_type, col_label_type> *wa) {
    int V = wa->graph.V;// Get the number of vertices in graph
    
    std::vector<unsigned short int> dist(V, USHRT_MAX);

    // minHeap represents set E
    MinHeap minHeap(V);
 
    // Initialize min heap with all vertices. dist value of all vertices 
    for (auto v = 0; v < V; ++v)
    {
        minHeap.array[v] = MinHeapNode(v, dist[v]);
        minHeap.pos[v] = v;
    }
 
    // Make dist value of src vertex as 0 so that it is extracted first
    minHeap.array[src] = MinHeapNode(src, dist[src]);
    minHeap.pos[src] = src;
    dist[src] = 0;
    minHeap.decreaseKey(src, dist[src]);
 
    // Initially size of min heap is equal to V
    minHeap.size = V;
 
    // In the followin loop, min heap contains all nodes
    // whose shortest distance is not yet finalized.
    while (!minHeap.isEmpty())
    {
        // Extract the vertex with minimum distance value
        auto minHeapNode = minHeap.extractMin();
        int u = minHeapNode.v; // Store the extracted vertex number
 
        // Traverse through all adjacent vertices of u (the extracted
        // vertex) and update their distance values
        AdjListNode* pCrawl = wa->graph.array[u].head;
        while (pCrawl != NULL)
        {
            int v = pCrawl->dest;
 
            // If shortest distance to v is not finalized yet, and distance to v
            // through u is less than its previously calculated distance
            if (minHeap.isInMinHeap(v) && (dist[u] != USHRT_MAX) && 
                                          (pCrawl->weight + dist[u] < dist[v]))
            {
                dist[v] = dist[u] + pCrawl->weight;
 
                // update distance value in min heap also
                minHeap.decreaseKey(v, dist[v]);
            }
            pCrawl = pCrawl->next;
        }
    }

    //calculate row and add to dataFrame
    calculateRow(dist, wa, src);
    
}

template<class row_label_type, class col_label_type>
void graphWorkerHandler(graphWorkerArgs<row_label_type,col_label_type>* wa)
{
    unsigned int src;
    bool endNow = false;
    while (!wa->jq.empty()) {
        src = wa->jq.pop(endNow);

        //exit loop if job queue was empty
        if (endNow) {
            break;
        }
        dijkstra(src, wa);
    }
}


namespace lmnoel {

// Initialization

    template<class row_label_type, class col_label_type>
    void transitMatrix<row_label_type, col_label_type>::prepareGraphWithVertices(int V)
    {
        numNodes = V;
        graph.initializeGraph(V);
    }

    template<class row_label_type, class col_label_type>
    void transitMatrix<row_label_type, col_label_type>::addToUserSourceDataContainer(unsigned int networkNodeId, const row_label_type& row_id, unsigned short int lastMileDistance)
    {
        unsigned int row_loc = df.addToRowIndex(row_id);
        this->userSourceDataContainer.addPoint(networkNodeId, row_loc, lastMileDistance);

    }

    template<class row_label_type, class col_label_type>
    void transitMatrix<row_label_type, col_label_type>::addToUserDestDataContainer(unsigned int networkNodeId, const col_label_type& col_id, unsigned short int lastMileDistance)
    {
        unsigned int col_loc = this->df.addToColIndex(col_id);
        this->userDestDataContainer.addPoint(networkNodeId, col_loc, lastMileDistance);
    }

    template<class row_label_type, class col_label_type>
    void transitMatrix<row_label_type, col_label_type>::addEdgeToGraph(int src, int dest, int weight, bool isBidirectional)
    {
        graph.addEdge(src, dest, weight);
        if (isBidirectional)
        {
            graph.addEdge(dest, src, weight);
        }
    }

    template<class row_label_type, class col_label_type>
    void transitMatrix<row_label_type, col_label_type>::addToCategoryMap(const col_label_type& dest_id, const std::string& category)
    {
        if (categoryToDestMap.find(category) != categoryToDestMap.end())
        {
            categoryToDestMap.at(category).push_back(dest_id);
        }
        else {
            std::vector<col_label_type> data;
            data.push_back(dest_id);
            categoryToDestMap.emplace(std::make_pair(category, data));
        }
    }

    // Calculations


    template<class row_label_type, class col_label_type>
    void transitMatrix<row_label_type, col_label_type>::compute(int numThreads)
    {
        try
        {
            graphWorkerArgs<row_label_type, col_label_type> wa(graph, userSourceDataContainer, userDestDataContainer,
                                                               numNodes, df);
            wa.initialize();
            workerQueue<row_label_type, col_label_type> wq(numThreads);
            wq.startGraphWorker(graphWorkerHandler, &wa);
        } catch (...)
        {
            throw std::runtime_error("Failed to compute matrix");
        }

    }

    template<class row_label_type, class col_label_type>
    const std::vector<std::pair<col_label_type, unsigned short int>> transitMatrix<row_label_type, col_label_type>::getValuesBySource(row_label_type source_id, bool sort) const
    {
        return this->df.getValuesByRowId(source_id, sort);
    }

    template<class row_label_type, class col_label_type>
    const std::vector<std::pair<row_label_type, unsigned short int>> transitMatrix<row_label_type, col_label_type>::getValuesByDest(col_label_type dest_id, bool sort) const
    {
        return this->df.getValuesByColId(dest_id, sort);
    }


    template<class row_label_type, class col_label_type>
    const std::unordered_map<row_label_type, std::vector<col_label_type>> transitMatrix<row_label_type, col_label_type>::getDestsInRange(unsigned int threshold, int numThreads) const
    {
        // Initialize maps
        std::unordered_map<row_label_type, std::vector<col_label_type>> destsInRange;
        for (unsigned int row_loc = 0; row_loc < df.getRows(); row_loc++)
        {
            std::vector<col_label_type> valueData;
            for (unsigned int col_loc = 0; col_loc < df.getCols(); col_loc++) {
                if (df.getValueByLoc(row_loc, col_loc) <= threshold) {
                    valueData.push_back(df.getColIdForLoc(col_loc));
                }
            }
            row_label_type row_id = df.getRowIdForLoc(row_loc);
            destsInRange.emplace(std::make_pair(row_id, valueData));
        }
        return destsInRange;

    }

    template<class row_label_type, class col_label_type>
    const std::unordered_map<col_label_type, std::vector<row_label_type>> transitMatrix<row_label_type, col_label_type>::getSourcesInRange(unsigned int threshold, int numThreads) const
    {
        // Initialize maps
        std::unordered_map<col_label_type, std::vector<row_label_type>> sourcesInRange;
        for (unsigned int col_loc = 0; col_loc < df.getCols(); col_loc++)
        {
            std::vector<row_label_type> valueData;
            for (unsigned int row_loc = 0; row_loc < df.getRows(); row_loc++) {
                if (df.getValueByLoc(row_loc, col_loc) <= threshold) {
                    valueData.push_back(df.getRowIdForLoc(row_loc));
                }
            }
            col_label_type col_id = df.getColIdForLoc(col_loc);
            sourcesInRange.emplace(std::make_pair(col_id, valueData));
        }
        return sourcesInRange;

    }

    template<class row_label_type, class col_label_type>
    unsigned short int transitMatrix<row_label_type, col_label_type>::timeToNearestDestPerCategory(const row_label_type& source_id, const std::string& category) const
    {
        unsigned short int minimum = USHRT_MAX;
        for (const col_label_type dest_id : categoryToDestMap.at(category))
        {
            unsigned short int dest_time = this->df.getValueById(source_id, dest_id);
            if (dest_time <= minimum)
            {
                minimum = dest_time;
            }
        }
        return minimum;
    }

    template<class row_label_type, class col_label_type>
    unsigned short int transitMatrix<row_label_type, col_label_type>::countDestsInRangePerCategory(const row_label_type& source_id, const std::string& category, unsigned short int range) const
    {
        unsigned short int count = 0;
        for (const col_label_type dest_id : categoryToDestMap.at(category))
        {
            if (this->df.getValueById(source_id, dest_id) <= range)
            {
                count++;
            }
        }
        return count;
    }

    template<class row_label_type, class col_label_type>
    unsigned short int transitMatrix<row_label_type, col_label_type>::timeToNearestDest(const row_label_type& source_id) const
    {
        unsigned short int minimum = USHRT_MAX;
        unsigned int row_loc = df.getRowLocForId(source_id);
        for (unsigned int col_loc = 0; col_loc < df.getCols(); col_loc++)
        {
            unsigned short int dest_time = this->df.getValueByLoc(row_loc, col_loc);
            if (dest_time <= minimum)
            {
                minimum = dest_time;
            }
        }
        return minimum;
    }

    template<class row_label_type, class col_label_type>
    unsigned short int transitMatrix<row_label_type, col_label_type>::countDestsInRange(const row_label_type& source_id, unsigned short int range) const
    {
        unsigned short int count = 0;
        unsigned int row_loc = df.getRowLocForId(source_id);
        for (unsigned int col_loc = 0; col_loc < df.getCols(); col_loc++)
        {
            if (this->df.getValueByLoc(row_loc, col_loc) <= range)
            {
                count++;
            }
        }
        return count;
    }

    // Getters

    template<class row_label_type, class col_label_type>
    unsigned short int transitMatrix<row_label_type, col_label_type>::getValueById(const row_label_type& row_id, const col_label_type& col_id) const
    {
        return df.getValueById(row_id, col_id);
    }


    template<class row_label_type, class col_label_type>
    unsigned int transitMatrix<row_label_type, col_label_type>::getRows() const {
        return df.getRows();
    }

    template<class row_label_type, class col_label_type>
    unsigned int transitMatrix<row_label_type, col_label_type>::getCols() const {
        return df.getCols();
    }

    template<class row_label_type, class col_label_type>
    bool transitMatrix<row_label_type, col_label_type>::getIsSymmetric() const {
        return df.getIsSymmetric();
    }

    template<class row_label_type, class col_label_type>
    const std::vector<std::vector<unsigned short int>>& transitMatrix<row_label_type, col_label_type>::getDataset() const
    {
        return df.getDataset();
    }

    template<class row_label_type, class col_label_type>
    const std::vector<row_label_type>& transitMatrix<row_label_type, col_label_type>::getPrimaryDatasetIds() const
    {
        return df.getRowIds();
    }

    template<class row_label_type, class col_label_type>
    const std::vector<col_label_type>& transitMatrix<row_label_type, col_label_type>::getSecondaryDatasetIds() const
    {
        return df.getColIds();
    }

    // Setters

    template<class row_label_type, class col_label_type>
    void transitMatrix<row_label_type, col_label_type>::setRows(unsigned int rows) {
        df.setRows(rows);
    }


    template<class row_label_type, class col_label_type>
    void transitMatrix<row_label_type, col_label_type>::setCols(unsigned int cols) {
        df.setCols(cols);
    }


    template<class row_label_type, class col_label_type>
    void transitMatrix<row_label_type, col_label_type>::setIsSymmetric(bool isSymmetric)
    {
        df.setIsSymmetric(isSymmetric);
    }

    template<class row_label_type, class col_label_type>
    void transitMatrix<row_label_type, col_label_type>::setDataset(const std::vector<std::vector<unsigned short int>>& dataset)
    {
        df.setDataset(dataset);
    }


    template<class row_label_type, class col_label_type>
    void transitMatrix<row_label_type, col_label_type>::setPrimaryDatasetIds(const std::vector<row_label_type>& primaryDatasetIds)
    {
        df.setRowIds(primaryDatasetIds);
    }

    template<class row_label_type, class col_label_type>
    void transitMatrix<row_label_type, col_label_type>::setSecondaryDatasetIds(const std::vector<col_label_type>& secondaryDatasetIds)
    {
        df.setColIds(secondaryDatasetIds);
    }



    // IO

    template<class row_label_type, class col_label_type>
    void transitMatrix<row_label_type, col_label_type>::printDataFrame() const
    {
        this->df.printDataFrame();
    }

    template<class row_label_type, class col_label_type>
    bool transitMatrix<row_label_type, col_label_type>::writeCSV(const std::string &outfile)
    {
        try {
            return this->df.writeCSV(outfile);
        }
        catch (...)
        {
            throw std::runtime_error("Unable to write csv");
        }
    }

} // namespace lnoel