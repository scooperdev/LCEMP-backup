#include "stdafx.h"
#include "File.h"
#include "RegionFileCache.h"
#include "ConsoleSaveFileIO.h"

RegionFileCache RegionFileCache::s_defaultCache;

bool RegionFileCache::useSplitSaves(ESavePlatform platform)
{
	switch(platform)
	{
	case SAVE_FILE_PLATFORM_XBONE:
	case SAVE_FILE_PLATFORM_PS4:
		return true;
	default:
		return false;
	};
}

RegionFile *RegionFileCache::_getRegionFile(ConsoleSaveFile *saveFile, const wstring &prefix, int chunkX, int chunkZ)
{
	// 4J Jev - changed back to use of the File class.
	MemSect(31);
	File file;
	if(useSplitSaves(saveFile->getSavePlatform()))
	{
		file = File( prefix + wstring(L"r.") + _toString(chunkX>>4) + L"." + _toString(chunkZ>>4) + L".mcr" );
	}
	else
	{
		file = File( prefix + wstring(L"r.") + _toString(chunkX>>5) + L"." + _toString(chunkZ>>5) + L".mcr" );
	}
	MemSect(0);

	EnterCriticalSection(&m_cs);

	RegionFile *ref = NULL;
	AUTO_VAR(it, cache.find(file));
	if( it != cache.end() )
		ref = it->second;

	// 4J Jev, put back in.
	if (ref != NULL)
	{
		LeaveCriticalSection(&m_cs);
		return ref;
	}

	if (cache.size() >= MAX_CACHE_SIZE)
	{
		_clear();
	}

	RegionFile *reg = new RegionFile(saveFile, &file);
	cache[file] = reg; 	   // 4J - this was originally a softReference
	LeaveCriticalSection(&m_cs);
	return reg;
}

void RegionFileCache::_clear()															// 4J - synchronized (recursive CS is safe here)
{
	EnterCriticalSection(&m_cs);
	AUTO_VAR(itEnd, cache.end());
	for( AUTO_VAR(it, cache.begin()); it != itEnd; it++ )
	{
        RegionFile *regionFile = it->second;
        if (regionFile != NULL)
		{
            regionFile->close();
        }
		delete regionFile;
	}
	cache.clear();
	LeaveCriticalSection(&m_cs);
}

int RegionFileCache::_getSizeDelta(ConsoleSaveFile *saveFile, const wstring &prefix, int chunkX, int chunkZ)
{
    RegionFile *r = _getRegionFile(saveFile, prefix, chunkX, chunkZ);
    return r->getSizeDelta();
}

DataInputStream *RegionFileCache::_getChunkDataInputStream(ConsoleSaveFile *saveFile, const wstring &prefix, int chunkX, int chunkZ)
{
	RegionFile* r = _getRegionFile(saveFile, prefix, chunkX, chunkZ);
	if(useSplitSaves(saveFile->getSavePlatform()))
	{
		return r->getChunkDataInputStream(chunkX & 15, chunkZ & 15);
	}
	else
	{

		return r->getChunkDataInputStream(chunkX & 31, chunkZ & 31);
	}
}

DataOutputStream *RegionFileCache::_getChunkDataOutputStream(ConsoleSaveFile *saveFile, const wstring &prefix, int chunkX, int chunkZ)
{
	RegionFile* r = _getRegionFile(saveFile, prefix, chunkX, chunkZ);
	if(useSplitSaves(saveFile->getSavePlatform()))
	{
		return r->getChunkDataOutputStream(chunkX & 15, chunkZ & 15);
	}
	else
	{

		return r->getChunkDataOutputStream(chunkX & 31, chunkZ & 31);
	}
}
