from libcpp.string cimport string
from libcpp cimport bool


cdef extern from "src/transitMatrix.h" namespace "lmnoel":

    cdef cppclass transitMatrix:
        transitMatrix(string) except +
        transitMatrix(int) except +
        void addToUserSourceDataContainer(int, string, int, bool) except +
        void addToUserDestDataContainer(int, string, int) except +
        void addEdgeToGraph(int, int, int, bool) except +
        void compute(int) except +
        int get(string, string) except +
        bool writeCSV(string) except +
        void printDataFrame() except +

cdef class pyTransitMatrix:
    cdef transitMatrix *thisptr

    def __cinit__(self, string infile = "", int vertices = -1):
        if vertices > 0:
            self.thisptr = new transitMatrix(vertices)
        else:

            self.thisptr = new transitMatrix(infile)

    def __dealloc__(self):
        del self.thisptr

    def addToUserSourceDataContainer(self, networkNodeId, id_, lastMileDistance, isBidirectional):
        self.thisptr.addToUserSourceDataContainer(networkNodeId, id_, lastMileDistance, isBidirectional)

    def addToUserDestDataContainer(self, networkNodeId, id_, lastMileDistance):
        self.thisptr.addToUserDestDataContainer(networkNodeId, id_, lastMileDistance)

    def addEdgeToGraph(self, src, dst, weight, isBidirectional):
        self.thisptr.addEdgeToGraph(src, dst, weight, isBidirectional)

    def compute(self, numThreads):
        self.thisptr.compute(numThreads)

    def get(self, source, dest):
        return self.thisptr.get(source, dest)

    def writeCSV(self, outfile):
        return self.thisptr.writeCSV(outfile)

    def printDataFrame(self):
        self.thisptr.printDataFrame()
