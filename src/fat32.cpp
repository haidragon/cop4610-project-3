#include "fat32.h"

// REMOVE
#include <unistd.h>

using namespace FAT_FS;

/**
 * FAT32 Public Methods
 */

/**
 * FAT32 Constructor
 * Description: Initializes a FAT32 object reading in file system info as
 *				well as finding currently free clusters.
 */
FAT32::FAT32( fstream & fatImage ) : fatImage( fatImage ) {

	// Read BIOS Parameter Block
	this->fatImage.seekg( 0 );
	this->fatImage.read( reinterpret_cast<char *>( &this->bpb ), sizeof( this->bpb ) );

	// Read FSInfo
	this->fatImage.seekg( this->bpb.FSInfo * this->bpb.bytesPerSector );
	this->fatImage.read( reinterpret_cast<char *>( &this->fsInfo ), sizeof( this->fsInfo ) );

	this->firstDataSector = this->bpb.reservedSectorCount + ( this->bpb.numFATs * this->bpb.FATSz32 );
	this->fatLocation = this->bpb.reservedSectorCount * this->bpb.bytesPerSector;

	// Read in fat
	// Note: The extra 2 entries allocated are for the reserved clusters which the countOfClusters formula 
	// 		 doesn't account for
	this->countOfClusters = ( ( this->bpb.totalSectors32 - this->firstDataSector ) / this->bpb.sectorsPerCluster );
	this->fat = new uint32_t[this->countOfClusters + 2];
	this->fatImage.seekg( this->fatLocation );
	this->fatImage.read( reinterpret_cast<char *>( this->fat ), ( this->countOfClusters + 2 ) *  FAT_ENTRY_SIZE );

	// Find free clusters
	// Note: We ignore the 2 reserved clusters and therefore also check the last 2
	uint32_t entry;
	uint32_t range = this->countOfClusters + 2;
	for ( uint32_t i = 2; i < range; i++ )
		if ( isFreeCluster( ( entry = getFATEntry( i ) ) ) )
			this->freeClusters.push_back( i );

	// Position ourselves in root directory
	this->currentDirectoryListing = getDirectoryListing( this->bpb.rootCluster );
}

/**
 * FAT32 Destructor
 */
FAT32::~FAT32() {

	// Cleanup
	delete[] this->fat;
}

/**
 * Get Current Path
 * Description: Builds and returns a / separated path to the
 *				current directory.
 */
const string FAT32::getCurrentPath() const {

	string path = "/";

	for ( uint32_t i = 0; i < this->currentPath.size(); i++ )
		path += this->currentPath[i] + "/";

	return path;
}

/**
 * Commands
 */

/**
 * FS Info
 * Description: Prints out info for the loaded FAT32 FS.
 */
void FAT32::fsinfo() const {

	// + used to promote type to a printable number
	cout << "Bytes per sector: " << this->bpb.bytesPerSector
		 << "\nSectors per cluster: " << +this->bpb.sectorsPerCluster
		 << "\nTotal sectors: " << this->bpb.totalSectors32
		 << "\nNumber of FATs: " << +this->bpb.numFATs
		 << "\nSectors per FAT: " << this->bpb.FATSz32
		 << "\nNumber of free sectors: " << this->freeClusters.size() * this->bpb.sectorsPerCluster
		 << "\n";
}

/**
 * Open File
 * Description: Attempts to open a file in the currrent directory 
 *				with r, w, or rw permissions and places it in
 *				the open file table.
 */
void FAT32::open( const string & fileName, const string & openMode ) {

	uint8_t mode;

	// Validate openMode
	if ( ( mode = isValidOpenMode( openMode ) ) == 0 ) {

		cout << "error: mode must be either r, w, rw.\n";
		return;
	}

	uint32_t index;

	// Try and find file
	if ( findFile( fileName, index ) ) {

		// Attempt to add file to open file table
		if ( this->openFiles.find( this->currentDirectoryListing[index] ) == this->openFiles.end() ) {

			this->openFiles[ this->currentDirectoryListing[index] ] = mode;
			cout << fileName << " has been opened with " << this->modeToString( mode ) << " permission.\n";
		}

		// File is already open
		else {

			cout << "error: " << fileName << " already open.\n";
			return;
		}
	}
}

/**
 * Close File
 * Description: Attempts to close a file that's in the current directory
 *				and open file table.
 */
void FAT32::close( const string & fileName ) {

	uint32_t index;

	// Try and find file
	if ( findFile( fileName, index ) ) {

		// Attempt to remove file from open file table
		if ( this->openFiles.find( this->currentDirectoryListing[index] ) != this->openFiles.end() ) {

			this->openFiles.erase( this->currentDirectoryListing[index] );
			cout << fileName << " is now closed.\n";
		}

		// File isn't in the table
		else {

			cout << "error: " << fileName << " not found in the open file table.\n";
			return;
		}
	}
}

/**
 * Create File
 * Description: TODO: Needs Description
 */
void FAT32::create() {

	cout << "error: unimplmented.\n";
}

/**
 * Read File
 * Description: Attempts to read a file if it's in the open file table. Reads
 *				the file starting at startPos and reads up to numBytes.
 */
void FAT32::read( const string & fileName, uint32_t startPos, uint32_t numBytes ) {

	uint32_t index;

	// Try and find file
	if ( findFile( fileName, index ) ) {

		DirectoryEntry file = this->currentDirectoryListing[index];

		// Check if file is already open
		if ( this->openFiles.find( file ) != this->openFiles.end() ) {

			// Check permissions
			if ( this->openFiles[file] == READ
				|| this->openFiles[file] == READWRITE ) {

				// Read file contents
				vector<uint32_t> clusterChain;
				uint8_t * contents = getFileContents( formCluster( file.shortEntry ), clusterChain );

				// Validate startPos against size
				if ( startPos >= file.shortEntry.fileSize )
					cout << "error: start_pos (" << startPos << ") greater than file size (" 
						<< file.shortEntry.fileSize << "). Note: start_pos is zero-based.\n";

				// Otherwise print file contents up to numBytes
				else
					for ( uint32_t i = 0; i < numBytes && startPos + i < file.shortEntry.fileSize; i++ )
						cout << contents[ startPos + i ];

				delete[] contents;
			
			} else {

				cout << "error: " << fileName << " not open for reading.\n";
				return;
			} 
		}

		else {

			cout << "error: " << fileName << " not found in the open file table.\n";
			return;
		}
	}
}

/**
 * Write to File
 * Description: TODO: Needs Description
 */
void FAT32::write() {

	cout << "error: unimplmented.\n";
}

/**
 * Remove File
 * Description: Removes a file from and updates all necessary records.
 *				Actually marks a file as free but doesn't zero out.
 */
void FAT32::rm( const string & fileName, bool safe ) {

	uint32_t index;

	// Try and find file
	if ( findFile( fileName, index ) ) {

		DirectoryEntry file = this->currentDirectoryListing[index];

		vector<uint32_t> clusterChain;

		// Check if we need to zero out file contents
		if ( safe )
			zeroOutFileContents( formCluster( file.shortEntry ) );

		uint32_t nextCluster = formCluster( file.shortEntry );

		// Build list of clusters ( potentially remaining if we crashed ) for this file
		do {

			clusterChain.push_back( nextCluster );

		// We must also check if the nextCluster is free because we may have crashed while deleting this
		} while ( ( nextCluster = getFATEntry( nextCluster ) ) < EOC && nextCluster != FREE_CLUSTER );	

		for ( vector<uint32_t>::reverse_iterator itr = clusterChain.rbegin(); itr != clusterChain.rend(); itr++ ) {

			setClusterValue( *itr, FREE_CLUSTER );
			this->freeClusters.push_back( *itr );
			this->fsInfo.freeCount++;
		}

		// Update all FATs
		for ( uint8_t i = 0; i < this->bpb.numFATs; i++ ) {

			uint32_t fatLocation = this->bpb.reservedSectorCount * this->bpb.bytesPerSector + ( i * this->bpb.FATSz32 * this->bpb.bytesPerSector );
			this->fatImage.seekp( fatLocation );
			this->fatImage.write( reinterpret_cast<char *>( this->fat ), ( this->countOfClusters + 2 ) *  FAT_ENTRY_SIZE );
		}

		// Update FSInfo
		this->fatImage.seekp( this->bpb.FSInfo * this->bpb.bytesPerSector );
		this->fatImage.write( reinterpret_cast<char *>( &this->fsInfo ), sizeof( this->fsInfo ) );

		// Delete directory entry
		for ( uint32_t i = 0; i < file.longEntries.size(); i++ ) {

			if ( safe )
				memset( &file.longEntries[i], 0, sizeof( file.longEntries[i] ) - sizeof( file.longEntries[i].location ) );

			file.longEntries[i].ordinal = DIR_FREE_ENTRY;
			deleteLongDirectoryEntry( file.longEntries[i] );
		}

		// Check if this is the last entry in a directory
		if ( safe )
				memset( &file.shortEntry, 0, sizeof( file.shortEntry ) - sizeof( file.shortEntry.location ) );
		file.shortEntry.name[0] = ( index + 1 == this->currentDirectoryListing.size() ) ? DIR_LAST_FREE_ENTRY : DIR_FREE_ENTRY; 
		deleteShortDirectoryEntry( file.shortEntry );

		// Don't let OS wait to flush
		this->fatImage.flush();

		this->currentDirectoryListing.erase( this->currentDirectoryListing.begin() + index );
	}
}

/**
 * Change Directory
 * Description: Attempts to change to a directory within the current directory.
 */
void FAT32::cd( const string & directoryName ) {

	uint32_t index;

	// Try and find directory
	if ( findDirectory( directoryName, index ) ) {

		// Check special case of .. directory and root
		if ( directoryName.compare( ".." ) == 0 && formCluster( currentDirectoryListing[index].shortEntry ) == 0 ) {

			this->currentPath.clear();
			this->currentDirectoryListing = getDirectoryListing( this->bpb.rootCluster );

		} else {

			// .. means we are going up a directory
			if ( directoryName.compare( ".." ) == 0 )
				this->currentPath.pop_back();

			// Don't add . to currentPath
			else if ( directoryName.compare( "." ) != 0 )
				this->currentPath.push_back( directoryName );

			this->currentDirectoryListing = getDirectoryListing( formCluster( currentDirectoryListing[index].shortEntry ) );
		}		
	}
}

/**
 * List Files
 * Description: Lists all files in either the current directory or 
 *				given directory if it exists.
 */
void FAT32::ls( const string & directoryName ) const {

	vector<DirectoryEntry> listing = currentDirectoryListing;

	// Check if we should list files of a given directory
	if ( !directoryName.empty() ) {

		uint32_t index;

		// Try and find directory
		if ( findDirectory( directoryName, index ) )
			listing = getDirectoryListing( formCluster( currentDirectoryListing[index].shortEntry ) );

		// Directory not found
		else
			return;
	}

	// Print directory contents
	for ( uint32_t i = 0; i < listing.size(); i++ )
		cout << listing[i].name << " ";

	cout << "\n";
}

/**
 * Make Directory
 * Description: TODO: Needs Description
 */
void FAT32::mkdir() {

	cout << "error: unimplmented.\n";
}

/**
 * Remove Directory
 * Description: TODO: Needs Description
 */
void FAT32::rmdir() {

	cout << "error: unimplmented.\n";
}

/**
 * Size of File
 * Description: Attempts to print the size of the file or directory given. 
 */
void FAT32::size( const string & entryName ) const {

	uint32_t index;

	// Try and find entry
	if ( findEntry( entryName, index ) ) {

		uint32_t totalSize = 0;
		DirectoryEntry entry = this->currentDirectoryListing[index];

		// Check if entry is a file
		if ( isFile( entry ) )
			totalSize = entry.shortEntry.fileSize;

		// Check if entry is a directory
		else if ( isDirectory( entry ) ) {

			// Get all of directory
			vector<uint32_t> clusterChain;
			uint8_t * contents = getFileContents( formCluster( entry.shortEntry ), clusterChain );
			uint32_t size = clusterChain.size() * ( this->bpb.sectorsPerCluster * this->bpb.bytesPerSector );

			// Calculate number of non-free entries
			for ( uint32_t i = 0; i < size; i += DIR_ENTRY_SIZE ) {

				uint8_t ordinal = contents[i];

				// Check if not free entry
				if ( ordinal != 0xE5 ) {

					// Rest of entries ahead of this are free
					if ( ordinal == 0x00 )
						break;

					totalSize += DIR_ENTRY_SIZE;
				}
			}

			delete[] contents;
		}

		// Otherwise it's a Volume ID
		else
			totalSize = 0;

		cout << totalSize << " bytes.\n";
	}

	cout << "error: TODO: waiting on comfirmation from daniel about sizing directory.\n";
}

/**
 * FAT32 Private Methods
 */

/**
 * Append Long Name
 * Description: Appends a LongDirectoryEntry's name to a string currently
 *				being built to represent the full name.
 */
void FAT32::appendLongName( string & current, uint16_t * name, uint32_t size ) const {

	// Iterate through name
	for ( uint32_t i = 0; i < size; i++ ) {

		// Check if current character is padding space or null terminator
		if ( name[i] == LONG_NAME_TRAIL || name[i] == LONG_NAME_NULL )
			break;

		// Otherwise add it to our string being built
		else {

			char temp = *(name + i);
			current += temp;
		}
	}
}

/**
 * Calculate Directory Entry Location
 * Description: Calculates exact byte location of a given directory entries relative byte
 *				to the directory with the cluster chain associated with it
 */
inline uint32_t FAT32::calculateDirectoryEntryLocation( uint32_t byte, vector<uint32_t> clusterChain ) const {

	return ( this->getFirstDataSectorOfCluster( 
				clusterChain[ byte / ( this->bpb.sectorsPerCluster * this->bpb.bytesPerSector ) ] ) * this->bpb.bytesPerSector 
		   ) + ( byte % ( this->bpb.sectorsPerCluster * this->bpb.bytesPerSector ) );
}

/**
 * Convert Short Name
 * Description: Converts a ShortEntry's name to a string. Also accounts
 *				for the implied '.'.
 */
const string FAT32::convertShortName( uint8_t * name ) const {

	string result = "";
	bool trailFound = false;
	bool connectionComplete = false;


	// Iterate through name
	for ( uint32_t i = 0; i < DIR_Name_Length; i++ ) {

		// Check if we have encountered padding
		if ( name[i] == SHORT_NAME_SPACE_PAD ) {

			trailFound = true;
			continue;
		}

		// Otherwise keep appending to current
		else {

			// If we found a trail and we haven't already
			// added the implied '.', add it
			if ( !connectionComplete && trailFound ) {

				connectionComplete = true;
				result += '.';
			}

			char temp = *(name + i);
			result += temp;
		}
	}

	return result;
}

/**
 * Delete Long Directory Entry
 * Description: Deletes a Long Directory Entry from the FS.
 */
inline void FAT32::deleteLongDirectoryEntry( LongDirectoryEntry entry ) {

	this->fatImage.seekp( entry.location );
	this->fatImage.write( reinterpret_cast<char *>( &entry ), DIR_ENTRY_SIZE );
}

/**
 * Delete Short Directory Entry
 * Description: Deletes a Short Directory Entry from the FS.
 */
inline void FAT32::deleteShortDirectoryEntry( ShortDirectoryEntry entry ) {

	this->fatImage.seekp( entry.location );
	this->fatImage.write( reinterpret_cast<char *>( &entry ), DIR_ENTRY_SIZE );
}

/**
 * Find Directory
 * Description: Returns whether or not a directory is in the current directory and
 *				if it is, the passed index is set.
 */
bool FAT32::findDirectory( const string & directoryName, uint32_t & index ) const {

	// Check if directoryName is valid
	if ( !isValidEntryName( directoryName ) ) {

		cout << "error: directory name may not contain /.\n";
		return false;
	}

	// Check if directory user want is in current directory
	for ( uint32_t i = 0; i < currentDirectoryListing.size(); i++ )
		if ( currentDirectoryListing[i].name.compare( directoryName ) == 0 ) {

			if ( isDirectory( currentDirectoryListing[i] ) )  {

				index = i;
				return true;
			}

			else {

				cout << "error: " << directoryName << " is not a directory.\n";
				return false;
			}
			
		}

	cout << "error: " << directoryName << " not found.\n";
	return false;
}

/**
 * Find Entry
 * Description: Returns whether or not an entry is in the current directory and
 *				if it is, the passed index is set.
 */
bool FAT32::findEntry( const string & entryName, uint32_t & index ) const {

	// Check if entryName is valid
	if ( !isValidEntryName( entryName ) ) {

		cout << "error: entry name may not contain /.\n";
		return false;
	}

	// Check if directory user want is in current directory
	for ( uint32_t i = 0; i < currentDirectoryListing.size(); i++ )
		if ( currentDirectoryListing[i].name.compare( entryName ) == 0 ) {

			index = i;
			return true;
		}

	cout << "error: " << entryName << " not found.\n";
	return false;
}

/**
 * Find File
 * Description: Returns whether or not a file is in the current directory and
 *				if it is, the passed index is set.
 */
bool FAT32::findFile( const string & fileName, uint32_t & index ) const {

	// Check if fileName is valid
	if ( !isValidEntryName( fileName ) ) {

		cout << "error: file name may not contain /.\n";
		return false;
	}

	// Check if directory user want is in current directory
	for ( uint32_t i = 0; i < currentDirectoryListing.size(); i++ )
		if ( currentDirectoryListing[i].name.compare( fileName ) == 0 ) {

			if ( isFile( currentDirectoryListing[i] ) )  {

				index = i;
				return true;
			}

			else {

				cout << "error: " << fileName << " is not a file.\n";
				return false;
			}
			
		}

	cout << "error: " << fileName << " not found.\n";
	return false;
}

/**
 * Form Cluster
 * Description: Concatenates the low and high order bits of a ShortDirectoryEntry
 *				to form the cluster number of that entry.
 */
inline uint32_t FAT32::formCluster( const ShortDirectoryEntry & entry ) const {

	uint32_t result = 0;
	result |= entry.firstClusterLO;
	result |= entry.firstClusterHI << 16;

	return result;
}

/**
 * Get Directory Listing
 * Description: Returns a list of DirectoryEntries for a given cluster.
 * Expects: cluster to be a valid data cluster.
 */
vector<DirectoryEntry> FAT32::getDirectoryListing( uint32_t cluster ) const {

	vector<uint32_t> clusterChain;
	uint8_t * contents = getFileContents( cluster, clusterChain );
	uint32_t size = clusterChain.size() * ( this->bpb.sectorsPerCluster * this->bpb.bytesPerSector );
	deque<LongDirectoryEntry> longEntries;
	vector<DirectoryEntry> result;

	// Parse contents
	for ( uint32_t i = 0; i < size; i += DIR_ENTRY_SIZE ) {

		uint8_t ordinal = contents[i];
		uint8_t attribute =  contents[i + DIR_Attr];

		// Check if not free entry
		if ( ordinal != DIR_FREE_ENTRY ) {

			// Rest of entries ahead of this are free
			if ( ordinal == DIR_LAST_FREE_ENTRY )
				break;

			// Check if this entry is a long directory
			if ( ( attribute & ATTR_LONG_NAME_MASK ) == ATTR_LONG_NAME ) {

				LongDirectoryEntry tempLongEntry;
				memcpy( &tempLongEntry, contents+i, sizeof( LongDirectoryEntry ) - sizeof( uint32_t ) );

				// Store this location in case we ever need to remove this entry
				tempLongEntry.location = calculateDirectoryEntryLocation( i, clusterChain );

				longEntries.push_front( tempLongEntry );

			// Otherwise it's a file
			} else {

				string name = "";
				uint8_t attr = attribute & ( ATTR_DIRECTORY | ATTR_VOLUME_ID );

				ShortDirectoryEntry tempShortEntry;
				memcpy( &tempShortEntry, contents+i, sizeof( ShortDirectoryEntry ) - sizeof( uint32_t ) );
				tempShortEntry.location = calculateDirectoryEntryLocation( i, clusterChain );

				// Build long entry name if there were any
				if ( !longEntries.empty() )
					for ( uint32_t i = 0; i < longEntries.size(); i++ ) {

						appendLongName( name, longEntries[i].name1, sizeof( longEntries[i].name1 )/2 );
						appendLongName( name, longEntries[i].name2, sizeof( longEntries[i].name2 )/2 );
						appendLongName( name, longEntries[i].name3, sizeof( longEntries[i].name3 )/2 );
					}

				else
					name = convertShortName( tempShortEntry.name );

				// Validate attribute
				if ( attr == 0x00 || attr == ATTR_DIRECTORY || attr == ATTR_VOLUME_ID ) {

					// Add new DirectoryEntry
					DirectoryEntry tempDirectoryEntry;
					tempDirectoryEntry.name = name;
					tempDirectoryEntry.fullPath = this->getCurrentPath() + name;
					tempDirectoryEntry.shortEntry = tempShortEntry;
					tempDirectoryEntry.longEntries = longEntries;
					result.push_back( tempDirectoryEntry );

				} else {

					// Invalid entry, ignore
				}

				// Reset long entries
				longEntries.clear();
			}
		}
	}

	delete[] contents;

	return result;
}

/**
 * Get FAT Entry
 * Description: Returns the value of a FAT entry without its upper 4 bits.
 */
inline uint32_t FAT32::getFATEntry( uint32_t n ) const {

	return this->fat[n] & FAT_ENTRY_MASK;
}

/**
 * Get File Contents
 * Description: Returns a buffer of the contents of a file starting at a given
 *				initial cluster. Also sets the cluster chain of this buffer. 
 *				This function will cause the program to abort if we are given 
 *				a cluster that leads to an empty chain.
 */
uint8_t * FAT32::getFileContents( uint32_t initialCluster, vector<uint32_t> & clusterChain ) const {

	uint32_t nextCluster = initialCluster;

	// Build list of clusters for this file
	do {

		clusterChain.push_back( nextCluster );

	} while ( ( nextCluster = getFATEntry( nextCluster ) ) < EOC );

	uint32_t size = clusterChain.size() * ( this->bpb.sectorsPerCluster * this->bpb.bytesPerSector );

	uint8_t * data = NULL;

	// Only allocate a buffer if we were able to make a chain
	if ( size > 0 ) {

		data = new uint8_t[ size ];
		uint8_t * temp = data;

		// Read in data
		for ( uint32_t i = 0; i < clusterChain.size(); i++ ) {

			this->fatImage.seekg( this->getFirstDataSectorOfCluster( clusterChain[i] ) * this->bpb.bytesPerSector  );

			for ( uint32_t j = 0; j < this->bpb.sectorsPerCluster ; j++ ) {

				this->fatImage.read( reinterpret_cast<char *>( temp ), this->bpb.bytesPerSector  );
				temp += this->bpb.bytesPerSector ;
			}
		}
	}

	if ( data == NULL ) {

		cout << "Failed to get file contents. Aborting.";
		exit( EXIT_SUCCESS );
	}

	return data;
}

/**
 * Get First Sector of Cluster
 * Description: Returns the first data sector of a given cluster.
 */
inline uint32_t FAT32::getFirstDataSectorOfCluster( uint32_t n ) const {

	return ( ( n - 2 ) * this->bpb.sectorsPerCluster ) + this->firstDataSector;
}

/**
 * Is Directory
 * Description: Checks if given entry is a directory.
 */
inline bool FAT32::isDirectory( const DirectoryEntry & entry ) const {

	return ( entry.shortEntry.attributes & ( ATTR_DIRECTORY | ATTR_VOLUME_ID ) ) == ATTR_DIRECTORY;
}

/**
 * Is File
 * Description: Checks if given entry is a file.
 */
inline bool FAT32::isFile( const DirectoryEntry & entry ) const {

	return ( entry.shortEntry.attributes & ( ATTR_DIRECTORY | ATTR_VOLUME_ID ) ) == 0x00;
}

/**
 * Is Free Cluster
 * Description: Checks if given cluster's value represents that it's free.
 */
inline bool FAT32::isFreeCluster( uint32_t value ) const {

	return ( value == FREE_CLUSTER );
}

/**
 * Is Valid Entry Name
 * Description: Checks if a given entry name is valid.
 */
inline bool FAT32::isValidEntryName( const string & entryName ) const {

	return entryName.find( "/" ) == string::npos;	
}

/**
 * Is Valid Open Mode
 * Description: Checks if a given file open mode is valid.
 */
inline uint8_t FAT32::isValidOpenMode( const string & openMode ) const {

	uint8_t result = 0;

	if ( openMode.compare( "r" ) == 0 )
		result = READ;

	else if ( openMode.compare( "w" ) == 0 )
		result = WRITE;

	else if ( openMode.compare( "rw" ) == 0 )
		result = READWRITE;

	return result;
}

/**
 * File Open Mode to String
 * Description: Converts an open mode to a representative string.
 */
inline const string FAT32::modeToString( const uint8_t & mode ) const {

	switch ( mode ) {

		case READ: return "reading";
		case WRITE: return "writing";
		case READWRITE: return "reading and writing";
	}

	return "invalid mode";
}

/**
 * Set Cluster Value
 * Description: Sets a cluster entry to a given value.
 */
inline void FAT32::setClusterValue( uint32_t n, uint32_t newValue ) {

	// Make sure we don't overwrite upper 4 bits
	newValue &= FAT_ENTRY_MASK;

	// Reset entry preserving upper 4 bits
	this->fat[n] &= ~FAT_ENTRY_MASK;

	// Set new value
	this->fat[n] |= newValue;
}

/**
 * Zero Out File Contents
 * Description: Zeros out a file for safety purposes.
 */
void FAT32::zeroOutFileContents( uint32_t initialCluster ) const {

	vector<uint32_t> clusterChain;
	uint32_t nextCluster = initialCluster;

	// Build list of clusters for this file
	do {

		clusterChain.push_back( nextCluster );

	} while ( ( nextCluster = getFATEntry( nextCluster ) ) < EOC );

	uint8_t * zeros = new uint8_t[this->bpb.bytesPerSector];
	memset( zeros, 0, this->bpb.bytesPerSector );

	// Write out zeros
	for ( uint32_t i = 0; i < clusterChain.size(); i++ ) {

		this->fatImage.seekp( this->getFirstDataSectorOfCluster( clusterChain[i] ) * this->bpb.bytesPerSector  );

		for ( uint32_t j = 0; j < this->bpb.sectorsPerCluster ; j++ )
			this->fatImage.write( reinterpret_cast<char *>( zeros ), this->bpb.bytesPerSector );
	}

	delete[] zeros;
}

/**
 * FAT_FS Functions
 */

/**
 * Less Than Operator for DirectoryEntry
 * Description: Used for std::map. A DirectoryEntry is considered less than
 *				another if its full path is less than the others.
 */
bool FAT_FS::operator< ( const DirectoryEntry & left, const DirectoryEntry & right ) {

	return operator<( left.fullPath, right.fullPath );
}
