/*
This file is part of Vlasiator.

Copyright 2010, 2011, 2012, 2013 Finnish Meteorological Institute

Vlasiator is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3
as published by the Free Software Foundation.

Vlasiator is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

/*! \file vlsvdiff.cpp
 * \brief Tool to compare VLSV files.
 * 
 * Tool to compare two VLSV files, two folders with the same number of VLSV files or a folder to a reference file.
 * The tool assumes the files have a name syntax 'grid\.[0-9]+\.vlsv'. It takes four arguments.
 * 
 * Calling patterns are:
 * 
 * "$ vlsvdiff <file1> <file2> <Variable> <component>": Gives single-file statistics and distances between the two files given, for the variable and component given
 * 
 * "$ vlsvdiff <folder1> <folder2> <Variable> <component>": Gives single-file statistics and distances between pairs of files grid*.vlsv taken in alphanumeric order in the two folders given, for the variable and component given
 * 
 * "$ vlsvdiff <file1> <folder2> <Variable> <component>" or "$ vlsvdiff <folder1> <file2> <Variable> <component>": Gives single-file statistics and distances between a file, and files grid*.vlsv taken in alphanumeric order in the given folder, for the variable and component given
 */

#include <cstdlib>
#include <iostream>
#include <stdint.h>
#include <cmath>
#include <limits> // YK
#include <list>
#include <set>
#include <sstream>
#include <dirent.h>
#include <typeinfo>

#include "vlsvreader2.h"
#include "definitions.h"
#include "vlsv_reader.h"
#include "vlsvreaderinterface.h"


using namespace std;
using namespace vlsv;

//O: NOTE: CHANGE THIS!
//The local cell ids are stored as a variable currently
template <class T>
bool getLocalCellIds( T & vlsvReader, 
                      const string & meshName, 
                      vector<uint64_t> & cellIds ) {
   if( !cellIds.empty() ) {
      cerr << "ERROR, cellIds must be empty at " << __FILE__ << " " << __LINE__ << endl;
   }

   datatype::type domainSizesDataType;
   uint64_t domainSizesArraySize, domainSizesVectorSize, domainSizesDataSize;

   //Create attribute list for reading in arrays (contains things like variable name and mesh name)
   //Read domain sizes: This is done to distinguish local cells and ghost cells in the array "MESH"
   list<pair<string, string> > domainSizesAttributes;
   domainSizesAttributes.push_back( make_pair("mesh", meshName) );
   //Retrieve info on the array:
   if (vlsvReader.getArrayInfo("MESH_DOMAIN_SIZES", domainSizesAttributes, domainSizesArraySize, domainSizesVectorSize, domainSizesDataType, domainSizesDataSize) == false) {
      cerr << "ERROR, failed to get array info from MESH_DOMAIN_SIZES at " << __FILE__ << " " << __LINE__ << endl;
      return false;
   }
   if( domainSizesDataType != datatype::type::UINT ) {
      cerr << "ERROR, bad datatype at: " << __FILE__ << " " << __LINE__ << endl;
   }
   if( domainSizesVectorSize != 2 ) {
      cerr << "ERROR, bad domain sizes vector size at: " << __FILE__ << " " << __LINE__ << endl;
   }

   //Read in the array:
   const int short domainSizesFirstIndex = 0;
   //Declare a buffer (info on the size was fetched with getArrayInfo)
   char * domainSizesBuffer = new char[domainSizesArraySize 
                                       * domainSizesVectorSize 
                                       * domainSizesDataSize / sizeof(char)];
   if (vlsvReader.readArray("MESH_DOMAIN_SIZES", domainSizesAttributes, domainSizesFirstIndex, domainSizesArraySize, domainSizesBuffer) == false) {
      cerr << "ERROR, failed to read MESH_DOMAIN_SIZES at " << __FILE__ << " " << __LINE__ << endl;
      return false;
   }
   uint32_t * domainSizes = reinterpret_cast<unsigned int*>(domainSizesBuffer);


   //Get the total amount of local cells:
   unsigned int numberOfLocalCells = 0;
   for( unsigned int i = 0; i < domainSizesArraySize; ++i ) {
      numberOfLocalCells += domainSizes[2 * i] - domainSizes[2 * i + 1];
   }
   //Reserve vector space for it:
   cellIds.reserve( numberOfLocalCells );


   //Get mesh info (for cell ids):
   datatype::type meshDataType;
   uint64_t meshVectorSize, meshArraySize, meshDataSize;
   list<pair<string, string> > meshAttributes;
   meshAttributes.push_back( make_pair("name", meshName) );
   if (vlsvReader.getArrayInfo("MESH", meshAttributes, meshArraySize, meshVectorSize, meshDataType, meshDataSize) == false) {
         cerr << "ERROR, failed to get info on array MESH at " << __FILE__ << " " << __LINE__ << endl;
         return false;
   }

   //Check that the data size is correct:
   if( meshDataSize != sizeof(uint64_t) ) {
      cerr << "ERROR, invalid data size at " << __FILE__ << " " << __LINE__ << endl;
      delete[] domainSizesBuffer;
      exit(1);
   }


   //Read the cell ids:
   unsigned int j = 0;
   uint64_t offsetIndex = 0;
   for( unsigned int i = 0; i < domainSizesArraySize; ++i ) {
      uint64_t howManyToRead;
      uint64_t howManyToSkip;
      howManyToRead = domainSizes[2 * i] - domainSizes[2 * i + 1];
      howManyToSkip = domainSizes[2 * i];

      //Read the cell ids:
      char * cellIds_buffer = new char[howManyToRead * meshDataSize * meshVectorSize];
      //The cell ids are of type uint64_t so reinterpret_cast the char pointer into readable form
      uint64_t * cellIds_uint_pointer = reinterpret_cast<uint64_t*>(cellIds_buffer);
      if (vlsvReader.readArray("MESH", meshAttributes, offsetIndex, howManyToRead, cellIds_buffer) == false) {
         cerr << "FAILED TO READ ARRAY MESH AT " << __FILE__ << " " << __LINE__ << endl;
         delete[] cellIds_buffer;
         delete[] domainSizesBuffer;
         exit(1);
      }

      //Push to the list of cells
      for( unsigned int j = 0; j < howManyToRead; ++j ) {
         //In visit cell ids start from 0 which is why they're stored like that in the file,
         //in vlasiator, they start from 1 -- hence the '+ 1'
         cellIds.push_back( (cellIds_uint_pointer[j]) + 1 );
      }
      delete[] cellIds_buffer;
      offsetIndex += howManyToSkip;
   }

   delete[] domainSizesBuffer;
   return true;
}


/*! Extracts the dataset from the VLSV file opened by convertSILO.
 * \param vlsvReader VLSVReader class object used to access the VLSV file
 * \param meshName Address of the string containing the name of the mesh to be extracted
 * \param varToExtract Pointer to the char array containing the name of the variable to extract
 * \param compToExtract Unsigned int designating the component to extract (0 for scalars)
 * \param orderedData Pointer to the return argument map which will get the extracted dataset
 */
bool convertMesh(newVlsv::Reader& vlsvReader,
                 const string& meshName,
                 const char * varToExtract,
                 const uint compToExtract,
                 map<uint, Real> * orderedData) {
   //Check for null pointer:
   if( !varToExtract || !orderedData ) {
      cerr << "ERROR, PASSED A NULL POINTER AT " << __FILE__ << " " << __LINE__ << endl;
      return false;
   }
   bool meshSuccess = true;
   bool variableSuccess = true;
   
   datatype::type meshDataType;
   datatype::type variableDataType;
   uint64_t meshArraySize, meshVectorSize, meshDataSize;
   uint64_t variableArraySize, variableVectorSize, variableDataSize;


   //Get local cell ids:
   vector<uint64_t> local_cells;
   getLocalCellIds( vlsvReader, meshName, local_cells );

   list<pair<string, string> > variableAttributes;
   const string _varToExtract( varToExtract );
   variableAttributes.push_back( make_pair("mesh", meshName) );
   variableAttributes.push_back( make_pair("name", _varToExtract) );
   //Read in array size, vector size, data type and data size of the array "VARIABLE" in the vlsv file (Needed in reading the array)
   if (vlsvReader.getArrayInfo("VARIABLE", variableAttributes, variableArraySize, variableVectorSize, variableDataType, variableDataSize) == false) {
      cerr << "ERROR, failed to read variable at " << __FILE__ << " " << __LINE__ << endl;
      return false;
   }


   //Check for correct output:
   if (local_cells.size() != variableArraySize) {
      cerr << "ERROR array size mismatch: " << local_cells.size() << " " << variableArraySize << endl;
   }
   if (compToExtract + 1 > variableVectorSize) {
      cerr << "ERROR invalid component, this variable has size " << variableVectorSize << endl;
      abort();
   }
   
   // Read the mesh array one node (of a spatial cell) at a time 
   // and create a map which contains each cell's CellID and variable to be extracted
   char* variableBuffer = new char[variableVectorSize*variableDataSize];
   float* variablePtrFloat = reinterpret_cast<float*>(variableBuffer);
   double* variablePtrDouble = reinterpret_cast<double*>(variableBuffer);
   uint* variablePtrUint = reinterpret_cast<uint*>(variableBuffer);
   int* variablePtrInt = reinterpret_cast<int*>(variableBuffer);
   
   for (uint64_t i=0; i<local_cells.size(); ++i) {
      const short int amountToReadIn = 1;
      const uint64_t & startingReadIndex = i;
      if (vlsvReader.readArray("VARIABLE", variableAttributes, startingReadIndex, amountToReadIn, variableBuffer) == false) {
         cerr << "ERROR, failed to read variable at " << __FILE__ << " " << __LINE__ << endl;
         variableSuccess = false; 
         break;
      }
      // Get the CellID
      uint64_t & CellID = local_cells[i];
      // Get the variable value
      Real extract = NAN;
      
      switch (variableDataType)
      {
         case datatype::type::FLOAT:
            if(variableDataSize == sizeof(float)) extract = (Real)(variablePtrFloat[compToExtract]);
            if(variableDataSize == sizeof(double)) extract = (Real)(variablePtrDouble[compToExtract]);
            break;
         case datatype::type::UINT:
            extract = (Real)(variablePtrUint[compToExtract]);
            break;
         case datatype::type::INT:
            extract = (Real)(variablePtrInt[compToExtract]);
            break;
         case datatype::type::UNKNOWN:
            cerr << "ERROR, BAD DATATYPE AT " << __FILE__ << " " << __LINE__ << endl;
            break;
      }
      // Put those into the map
      orderedData->insert(pair<uint64_t, Real>(CellID, extract));
   }
   
   if (meshSuccess == false) {
      cerr << "ERROR reading array MESH" << endl;
   }
   if (variableSuccess == false) {
      cerr << "ERROR reading array VARIABLE " << varToExtract << endl;
   }
   return meshSuccess && variableSuccess;
}


/*! Extracts the dataset from the VLSV file opened by convertSILO.
 * \param vlsvReader VLSVReader class object used to access the VLSV file
 * \param meshName Address of the string containing the name of the mesh to be extracted
 * \param varToExtract Pointer to the char array containing the name of the variable to extract
 * \param compToExtract Unsigned int designating the component to extract (0 for scalars)
 * \param orderedData Pointer to the return argument map which will get the extracted dataset
 */
bool convertMesh(VLSVReader & vlsvReader,
                 const string& meshName,
                 const char * varToExtract,
                 const uint compToExtract,
                 map<uint, Real> * orderedData)
{
   bool meshSuccess = true;
   bool variableSuccess = true;
   
   VLSV::datatype meshDataType;
   VLSV::datatype variableDataType;
   uint64_t meshArraySize, meshVectorSize, meshDataSize;
   uint64_t variableArraySize, variableVectorSize, variableDataSize;
   
   if (vlsvReader.getArrayInfo("MESH", meshName, meshArraySize, meshVectorSize, meshDataType, meshDataSize) == false) return false;
   if (vlsvReader.getArrayInfo("VARIABLE", varToExtract, meshName, variableArraySize, variableVectorSize, variableDataType, variableDataSize) == false) return false;
   if (meshArraySize != variableArraySize) {
      cerr << "ERROR array size mismatch" << endl;
   }
   if (compToExtract + 1 > variableVectorSize) {
      cerr << "ERROR invalid component, this variable has size " << variableVectorSize << endl;
      abort();
   }
   
   // Read the mesh array one node (of a spatial cell) at a time 
   // and create a map which contains each cell's CellID and variable to be extracted
   char* meshBuffer = new char[meshVectorSize*meshDataSize];
   uint64_t* meshPtr = reinterpret_cast<uint64_t*>(meshBuffer);
   char* variableBuffer = new char[variableVectorSize*variableDataSize];
   float* variablePtrFloat = reinterpret_cast<float*>(variableBuffer);
   double* variablePtrDouble = reinterpret_cast<double*>(variableBuffer);
   uint* variablePtrUint = reinterpret_cast<uint*>(variableBuffer);
   int* variablePtrInt = reinterpret_cast<int*>(variableBuffer);
   
   for (uint64_t i=0; i<meshArraySize; ++i) {
      if (vlsvReader.readArray("MESH", meshName, i, 1, meshBuffer) == false) {meshSuccess = false; break;}
      if (vlsvReader.readArray("VARIABLE", varToExtract, i, 1, variableBuffer) == false) {variableSuccess = false; break;}
      // Get the CellID
      uint64_t CellID  = meshPtr[0];
      // Get the variable value
      Real extract = NAN;
      
      switch (variableDataType)
      {
         case VLSV::FLOAT:
            if(variableDataSize == sizeof(float)) extract = (Real)(variablePtrFloat[compToExtract]);
            if(variableDataSize == sizeof(double)) extract = (Real)(variablePtrDouble[compToExtract]);
            break;
         case VLSV::UINT:
            extract = (Real)(variablePtrUint[compToExtract]);
            break;
         case VLSV::INT:
            extract = (Real)(variablePtrInt[compToExtract]);
            break;
      }
      // Put those into the map
      orderedData->insert(pair<uint64_t, Real>(CellID, extract));
   }
   
   if (meshSuccess == false) {
      cerr << "ERROR reading array MESH" << endl;
   }
   if (variableSuccess == false) {
      cerr << "ERROR reading array VARIABLE " << varToExtract << endl;
   }
   return meshSuccess && variableSuccess;
}

/* Checks if the vlsv file is using the new library or not
 * Input:
 * \param vlsvReader -- some vlsv reader with a file open
 * Output:
 * Returns true if the vlsv file has been written with the new vlsv library and false if not
 */
int checkVersion( Reader & vlsvReader ) {
   string versionTag = "version";
   float version;
   if( vlsvReader.readParameter( versionTag, version ) == false ) {
      return 0;
   }
   if( version == 1.00 ) {
      return 1;
   } else {
      cerr << "Invalid version!" << endl;
      exit(1);
      return 0;
   }
}


/*! Opens the VLSV file and extracts the mesh names. Sends for processing to convertMesh.
 * \param fileName String containing the name of the file to be processed
 * \param varToExtract Pointer to the char array containing the name of the variable to extract
 * \param compToExtract Unsigned int designating the component to extract (0 for scalars)
 * \param orderedData Pointer to the return argument map which will get the extracted dataset
 * \sa convertMesh
 */
template <class T>
bool convertSILO(const string fileName,
                 const char * varToExtract,
                 const uint compToExtract,
                 map<uint, Real> * orderedData)
{
   bool success = true;
   
   // Open VLSV file for reading:
   T vlsvReader;
   
   if (vlsvReader.open(fileName) == false)
   {
      cerr << "Failed to open '" << fileName << "'" << endl;
      return false;
   }
   
   // Get the names of all meshes in vlsv file
   list<string> meshNames;
   if (vlsvReader.getMeshNames(meshNames) == false) {
      cerr << "Failed to read mesh names" << endl;
      exit(1);
   }

   
   for (list<string>::const_iterator it=meshNames.begin(); it!=meshNames.end(); ++it)
   {
      if (convertMesh(vlsvReader, *it, varToExtract, compToExtract, orderedData) == false)
      {
         return false;
      }
      
   }
   vlsvReader.close();
   return success;
}

/*! Shift the second file to the average of the first
 * \param orderedData1 Pointer to the reference file's data
 * \param orderedData2 Pointer to the data to be shifted
 * \param shiftedData2 Pointer to where the shifted data of the second file will be put
 */
bool shiftAverage(const map<uint, Real> * const orderedData1,
                  const map<uint, Real> * const orderedData2,
                  map<uint, Real> * shiftedData2
                  ) {
   map<uint, Real>::const_iterator it1, it2;
   Real avg1 = 0.0;
   Real avg2 = 0.0;
   
   for(it1=orderedData1->begin(), it2=orderedData2->begin();
       it1 != orderedData1->end(), it2 != orderedData2->end();
       it1++, it2++)
   {
      avg1 += orderedData1->at(it1->first);
      avg2 += orderedData2->at(it2->first);
   }
   avg1 /= orderedData1->size();
   avg2 /= orderedData1->size();
   
   for(it2=orderedData2->begin(); it2 != orderedData2->end(); it2++)
   {
      shiftedData2->insert(pair<uint, Real>(it2->first, it2->second - avg2 + avg1));
   }
   
   return 0;
}

/*! Compute the absolute and relative \f$ p \f$-distance between two datasets X(x) provided in the maps orderedData1 and orderedData2. Note that the dataset passed in orderedData1 will be taken as the reference dataset both when shifting averages and when computing relative distances.
 * 
 * For \f$ p \neq 0 \f$:
 * 
 * absolute \f$ p \f$-distance defined as:
 * 
 * \f$ \|X_1 - X_2\|_p = \left[\sum_i |X_1(i) - X_2(i)|^p\right]^{1/p}\f$,
 * 
 * relative \f$ p \f$-distance defined as:
 * 
 * \f$ \|X_1 - X_2\|_p = \left[\sum_i |X_1(i) - X_2(i)|^p\right]^{1/p} / \|X_1\|_p \f$.
 * 
 * For \f$ p = 0 \f$ it is the \f$ \infty \f$-distance:
 * 
 * absolute \f$ \infty \f$-distance defined as:
 * 
 * \f$ \|X_1 - X_2\|_\infty = \max_i\left(|X_1(i) - X_2(i)|\right)\f$
 * 
 * relative \f$ \infty \f$-distance defined as:
 * 
 * \f$ \|X_1 - X_2\|_\infty = \max_i\left(|X_1(i) - X_2(i)|\right) / \|X_1\|_\infty \f$
 * 
 * \param orderedData1 Pointer to the first file's data map
 * \param orderedData2 Pointer to the second file's data map
 * \param p Parameter of the distance formula
 * \param absolute Return argument pointer, absolute value
 * \param relative Return argument pointer, relative value
 * \param doShiftAverage Boolean argument to determine whether to shift the second file's data
 * \sa shiftAverage
 */
bool pDistance(map<uint, Real> * const orderedData1,
               map<uint, Real> * const orderedData2,
               creal p,
               Real * absolute,
               Real * relative,
               const bool doShiftAverage
               ) {
   map<uint, Real> shiftedData2;
   map<uint, Real> * data2 = orderedData2;
   
   if(doShiftAverage == true)
   {
      shiftAverage(orderedData1, orderedData2, &shiftedData2);
      data2 = &shiftedData2;
   }
   
   *absolute = 0.0;
   Real length = 0.0;
   *relative = 0.0;
   map<uint, Real>::const_iterator it;
   if(p != 0.0)
   {
      for(it=orderedData1->begin(); it != orderedData1->end() ; it++)
      {
         *absolute += pow(abs(orderedData1->at(it->first) - data2->at(it->first)), p);
         length += pow(abs(orderedData1->at(it->first)), p);
      }
      *absolute = pow(*absolute, 1.0 / p);
      length = pow(length, 1.0 / p);
   }
   else
   {
      for(it=orderedData1->begin(); it != orderedData1->end() ; it++)
      {
         *absolute = max(*absolute, abs(orderedData1->at(it->first) - data2->at(it->first)));
         length = max(length, abs(orderedData1->at(it->first)));
      }
   }
   
   if(length != 0.0)
   {
      *relative = *absolute / length;
   }
   else
   {
      cout << "WARNING (pDistance) : length of reference is 0.0, cannot divide to give relative distance." << endl;
      *relative = -1;
   }
   return 0;
}

/*! In verbose mode print the distance, in non-verbose store them for later output when lastCall is true
 * \param p Parameter of the distance
 * \param absolute Absolute value pointer
 * \param relative Relative value pointer
 * \param shiftedAverage Boolean parameter telling whether the dataset is average-shifted
 * \param verboseOutput Boolean parameter telling whether the output is verbose or compact
 * \param lastCall Boolean parameter telling whether this is the last call to the function
 * \sa shiftAverage pDistance
 */
bool outputDistance(const Real p,
                    const Real * absolute,
                    const Real * relative,
                    const bool shiftedAverage,
                    const bool verboseOutput,
                    const bool lastCall
)
{
   if(verboseOutput == true) {
      if(shiftedAverage == false) {
         cout << "The absolute " << p << "-distance between both datasets is " << *absolute  << endl;
         cout << "The relative " << p << "-distance between both datasets is " << *relative  << endl;
      } else {
         cout << "The average-shifted absolute " << p << "-distance between both datasets is " << *absolute  << endl;
         cout << "The average-shifted relative " << p << "-distance between both datasets is " << *relative  << endl;
      }
   } else {
      static vector<Real> fileOutputData;
      static uint fileNumber = 0;
      
      if(lastCall == true) {
         vector<Real>::const_iterator it;
         for(it = fileOutputData.begin(); it != fileOutputData.end(); it++) {
            cout << *it << "\t";
         }
         fileOutputData.clear();
         return 0;
      }
      
      fileOutputData.push_back(*absolute);
      fileOutputData.push_back(*relative);
   }
   return 0;
}

/*! Compute statistics on a single file
 * \param size Return argument pointer, dataset size
 * \param mini Return argument pointer, dataset minimum
 * \param maxi Return argument pointer, dataset maximum
 * \param avg Return argument pointer, dataset average
 * \param stdev Return argument pointer, dataset standard deviation
 */
bool singleStatistics(map<uint, Real> * orderedData,
                      Real * size,
                      Real * mini,
                      Real * maxi,
                      Real * avg,
                      Real * stdev
)
{
   /*
    * Returns basic statistics on the map passed to it.
    */
   map<uint, Real>::const_iterator it;
   
   *size = orderedData->size();
   *mini = numeric_limits<Real>::max();
   *maxi = numeric_limits<Real>::min();
   *avg = 0.0;
   *stdev = 0.0;
   
   for(it=orderedData->begin(); it != orderedData->end() ; it++)
   {
      *mini = min(*mini, orderedData->at(it->first));
      *maxi = max(*maxi, orderedData->at(it->first));
      *avg += orderedData->at(it->first);
   }
   *avg /= *size;
   for(it=orderedData->begin(); it != orderedData->end() ; it++)
   {
      *stdev += pow(orderedData->at(it->first) - *avg, 2.0);
   }
   *stdev = sqrt(*stdev);
   *stdev /= (*size - 1);
   return 0;
}

/*! In verbose mode print the statistics, in non-verbose store them for later output when lastCall is true
 * \param size Pointer to dataset size
 * \param mini Pointer to dataset minimum
 * \param maxi Pointer to dataset maximum
 * \param avg Pointer to dataset average
 * \param stdev Pointer to dataset standard deviation
 * \param verboseOutput Boolean parameter telling whether the output is verbose or compact
 * \param lastCall Boolean parameter telling whether this is the last call to the function
 * \sa singleStatistics
 */
bool outputStats(const Real * size,
                 const Real * mini,
                 const Real * maxi,
                 const Real * avg,
                 const Real * stdev,
                 const bool verboseOutput,
                 const bool lastCall
                 ) {
   if(verboseOutput == true)
   {
      cout << "Statistics on file: size " << *size
      << " min = " << *mini
      << " max = " << *maxi
      << " average = " << *avg
      << " standard deviation " << *stdev
      << endl;
   }
   else
   {
      static uint fileNumber = 0;
      static vector<Real> pairStats;
      
      if(lastCall == true)
      {
         vector<Real>::const_iterator it;
         for(it = pairStats.begin(); it != pairStats.end(); it++)
         {
            cout << *it << "\t";
         }
         pairStats.clear();
         return 0;
      }
      
      if(fileNumber%2 == 0)
      {
         pairStats.push_back(fileNumber / 2 + 1);
      }
      pairStats.push_back(*size);
      pairStats.push_back(*mini);
      pairStats.push_back(*maxi);
      pairStats.push_back(*avg);
      pairStats.push_back(*stdev);
      fileNumber++;
   }
   return 0;
}

/*! In folder-processing, non-verbose mode the data are stored during the processing and output at the end to have the data sorted properly
 * \sa outputStats outputDistance
 */
bool printNonVerboseData()
{
   static bool header = true;
   if(header == true)
   {
      // Key to contents
      cout << "#1   File number in folder\n" <<
              "#2   File 1 size\n" <<
              "#3   File 1 min\n" <<
              "#4   File 1 max\n" <<
              "#5   File 1 average\n" <<
              "#6   File 1 standard deviation\n" <<
              "#7   File 2 size\n" <<
              "#8   File 2 min\n" <<
              "#9   File 2 max\n" <<
              "#10  File 2 average\n" <<
              "#11  File 2 standard deviation\n" <<
              "#12  absolute infinity-distance\n" <<
              "#13  relative infinity-distance\n" <<
              "#14  absolute average-shifted infinity-distance\n" <<
              "#15  relative average-shifted infinity-distance\n" <<
              "#16  absolute 1-distance\n" <<
              "#17  relative 1-distance\n" <<
              "#18  absolute average-shifted 1-distance\n" <<
              "#19  relative average-shifted 1-distance\n" <<
              "#20  absolute 2-distance\n" <<
              "#21  relative 2-distance\n" <<
              "#22  absolute average-shifted 2-distance\n" <<
              "#23  relative average-shifted 2-distance\n" <<
              endl;
      header = false;
   }
   
   // Data
   // last argument (lastCall) is true to get the output of the whole stored dataset
   outputStats(NULL, NULL, NULL, NULL, NULL, false, true);
   outputDistance(0, NULL, NULL, false, false, true);
   
   return 0;
}

/*! Read in the contents of the variable component in both files passed in strings fileName1 and fileName2, and compute statistics and distances as wished
 * \param fileName1 String argument giving the location of the first file to process
 * \param fileName2 String argument giving the location of the second file to process
 * \param varToExtract Pointer to the char array containing the name of the variable to extract
 * \param compToExtract Unsigned int designating the component to extract (0 for scalars)
 * \param verboseOutput Boolean parameter telling whether the output will be verbose or compact
 * \sa convertSILO singleStatistics outputStats pDistance outputDistance printNonVerboseData
 */
bool process2Files(const string fileName1,
                   const string fileName2,
                   const char * varToExtract,
                   const uint compToExtract,
                   const bool verboseOutput
                  ) {
   map<uint, Real> orderedData1;
   map<uint, Real> orderedData2;
   Real absolute, relative, mini, maxi, size, avg, stdev;


   //Check whether the file(s) use new or old vlsv library:
   Reader vlsvCheck1;
   vlsvCheck1.open( fileName1 );
   const bool file1UsesNewVlsvLib = (checkVersion( vlsvCheck1 ) == 1.00);
   if( file1UsesNewVlsvLib == false )
   vlsvCheck1.close();

   Reader vlsvCheck2;
   vlsvCheck2.open( fileName2 );
   const bool file2UsesNewVlsvLib = (checkVersion( vlsvCheck2 ) == 1.00);
   if( file1UsesNewVlsvLib == false )
   vlsvCheck2.close();

   bool success = true;
   if( file1UsesNewVlsvLib ) {
      success = convertSILO<newVlsv::Reader>(fileName1, varToExtract, compToExtract, &orderedData1);
   } else {
      success = convertSILO<VLSVReader>(fileName1, varToExtract, compToExtract, &orderedData1);
   }
   if( success == false ) {
      cerr << "ERROR Data import error with " << fileName1 << endl;
      return 1;
   }

   if( file2UsesNewVlsvLib ) {
      success = convertSILO<newVlsv::Reader>(fileName2, varToExtract, compToExtract, &orderedData2);
   } else {
      success = convertSILO<VLSVReader>(fileName2, varToExtract, compToExtract, &orderedData2);
   }
   if( success == false ) {
      cerr << "ERROR Data import error with " << fileName2 << endl;
      return 1;
   }


   // Basic consistency check
   if(orderedData1.size() != orderedData2.size()) {
      cerr << "ERROR Datasets have different size." << endl;
      return 1;
   }
   
   singleStatistics(&orderedData1, &size, &mini, &maxi, &avg, &stdev);
   outputStats(&size, &mini, &maxi, &avg, &stdev, verboseOutput, false);
   
   singleStatistics(&orderedData2, &size, &mini, &maxi, &avg, &stdev);
   outputStats(&size, &mini, &maxi, &avg, &stdev, verboseOutput, false);
   
   pDistance(&orderedData1, &orderedData2, 0, &absolute, &relative, false);
   outputDistance(0, &absolute, &relative, false, verboseOutput, false);
   pDistance(&orderedData1, &orderedData2, 0, &absolute, &relative, true);
   outputDistance(0, &absolute, &relative, true, verboseOutput, false);
   
   pDistance(&orderedData1, &orderedData2, 1, &absolute, &relative, false);
   outputDistance(1, &absolute, &relative, false, verboseOutput, false);
   pDistance(&orderedData1, &orderedData2, 1, &absolute, &relative, true);
   outputDistance(1, &absolute, &relative, true, verboseOutput, false);
   
   pDistance(&orderedData1, &orderedData2, 2, &absolute, &relative, false);
   outputDistance(2, &absolute, &relative, false, verboseOutput, false);
   pDistance(&orderedData1, &orderedData2, 2, &absolute, &relative, true);
   outputDistance(2, &absolute, &relative, true, verboseOutput, false);
   
   if(verboseOutput == false)
   {
      printNonVerboseData();
      cout << endl;
   }
   
   return 0;
}

/*! Creates the list of grid*.vlsv files present in the folder passed
 * \param dir DIR type pointer to the directory entry to process
 * \param fileList Pointer to a set of strings, return argument for the produced file list
 */
bool processDirectory(DIR* dir, set<string> * fileList) {
   int filesFound = 0, entryCounter = 0;
   
   const string mask = "grid";
   const string suffix = ".vlsv";
   
   struct dirent* entry = readdir(dir);
   while (entry != NULL) {
      const string entryName = entry->d_name;
      if (entryName.find(mask) == string::npos || entryName.find(suffix) == string::npos) {
         entry = readdir(dir);
         continue;
      }
      fileList->insert(entryName);
      filesFound++;
      entry = readdir(dir);
   }
   if (filesFound == 0) cout << "INFO no matches found" << endl;
   
   return 0;
}

/*! Main function, detects which calling pattern is used and sends to the corresponding processing functions.
 * 
 * \sa process2Files processDirectory
 */
int main(int argn,char* args[])
{
   if (argn < 5)
   {
      cout << endl;
      cout << "USAGE 1: ./vlsvdiff <file1> <file2> <Variable> <component>" << endl;
      cout << "Gives single-file statistics and distances between the two files given, for the variable and component given" << endl;
      cout << "USAGE 2: ./vlsvdiff <folder1> <folder2> <Variable> <component>" << endl;
      cout << "Gives single-file statistics and distances between pairs of files grid*.vlsv taken in alphanumeric order in the two folders given, for the variable and component given" << endl;
      cout << "USAGE 3: ./vlsvdiff <file1> <folder2> <Variable> <component>" << endl;
      cout << "         ./vlsvdiff <folder1> <file2> <Variable> <component>" << endl;
      cout << "Gives single-file statistics and distances between a file, and files grid*.vlsv taken in alphanumeric order in the given folder, for the variable and component given" << endl;
      cout << endl;
      return 1;
   }
   
   // 1st arg is file1 name
   const string fileName1 = args[1];
   // 2nd arg is file2 name
   const string fileName2 = args[2];
   // 3rd arg is variable name
   char * varToExtract = args[3];
   // 4th arg is its component, 0 for scalars, 2 for z component etc
   uint compToExtract = atoi(args[4]);
   
   DIR* dir1 = opendir(fileName1.c_str());
   DIR* dir2 = opendir(fileName2.c_str());
   
   if (dir1 == NULL && dir2 == NULL) {
      cout << "INFO Reading in two files." << endl;
      
      // Process two files with verbose output (last argument true)
      process2Files(fileName1, fileName2, varToExtract, compToExtract, true);
      //CONTINUE
      
      closedir(dir1);
      closedir(dir2);
   }
   else if (dir1 == NULL || dir2 == NULL)
   {
      // Mixed file and directory
      cout << "#INFO Reading in one file and one directory." << endl;
      set<string> fileList;
      set<string>::iterator it;

      if(dir1 == NULL){
         //file in 1, directory in 2
         processDirectory(dir2, &fileList);
         for(it = fileList.begin(); it != fileList.end();++it){
            // Process two files with non-verbose output (last argument false), give full path to the file processor
            process2Files(fileName1,fileName2 + "/" + *it, varToExtract, compToExtract, false);
         }
      }

      if(dir2 == NULL){
         //directory in 1, file in 2
         processDirectory(dir1, &fileList);
         for(it = fileList.begin(); it != fileList.end();++it){
            // Process two files with non-verbose output (last argument false), give full path to the file processor
            process2Files(fileName1+"/"+*it,fileName2, varToExtract, compToExtract, false);
         }
      }

      closedir(dir1);
      closedir(dir2);
      return 1;
   }
   else if (dir1 != NULL && dir2 != NULL)
   {
      // Process two folders, files of the same rank compared, first folder is reference in relative distances
      cout << "#INFO Reading in two directories." << endl;
      set<string> fileList1, fileList2;
      
      // Produce a sorted file list
      processDirectory(dir1, &fileList1);
      processDirectory(dir2, &fileList2);
      
      // Basic consistency check
      if(fileList1.size() != fileList2.size())
      {
         cerr << "ERROR Folders have different number of files." << endl;
         return 1;
      }
      
      set<string>::iterator it1, it2;
      for(it1 = fileList1.begin(), it2 = fileList2.begin();
          it1 != fileList2.end(), it2 != fileList2.end();
          it1++, it2++)
      {
      // Process two files with non-verbose output (last argument false), give full path to the file processor
      process2Files(fileName1 + "/" + *it1,
                    fileName2 + "/" + *it2, varToExtract, compToExtract, false);
      }
      
      closedir(dir1);
      closedir(dir2);
   }
   
   return 0;
}
