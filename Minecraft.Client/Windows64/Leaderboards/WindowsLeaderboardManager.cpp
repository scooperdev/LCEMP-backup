#include "stdafx.h"

#include "WindowsLeaderboardManager.h"

#include "../../StatsCounter.h"

#include "../../../Minecraft.World/Stats.h"
#include "../../../Minecraft.World/net.minecraft.world.item.h"
#include "../../../Minecraft.World/net.minecraft.world.level.tile.h"

namespace
{
	static bool HasAnyTrackedLeaderboardProgress(StatsCounter *statsCounter)
	{
		if (statsCounter == NULL)
		{
			return false;
		}

		for (int difficulty = 0; difficulty < 4; ++difficulty)
		{
			if (statsCounter->getValue(Stats::walkOneM, difficulty) > 0
				|| statsCounter->getValue(Stats::fallOneM, difficulty) > 0
				|| statsCounter->getValue(Stats::minecartOneM, difficulty) > 0
				|| statsCounter->getValue(Stats::boatOneM, difficulty) > 0
				|| statsCounter->getValue(Stats::blocksMined[Tile::dirt->id], difficulty) > 0
				|| statsCounter->getValue(Stats::blocksMined[Tile::stoneBrick->id], difficulty) > 0
				|| statsCounter->getValue(Stats::blocksMined[Tile::sand->id], difficulty) > 0
				|| statsCounter->getValue(Stats::blocksMined[Tile::rock->id], difficulty) > 0
				|| statsCounter->getValue(Stats::blocksMined[Tile::gravel->id], difficulty) > 0
				|| statsCounter->getValue(Stats::blocksMined[Tile::clay->id], difficulty) > 0
				|| statsCounter->getValue(Stats::blocksMined[Tile::obsidian->id], difficulty) > 0
				|| statsCounter->getValue(Stats::itemsCollected[Item::egg->id], difficulty) > 0
				|| statsCounter->getValue(Stats::blocksMined[Tile::crops_Id], difficulty) > 0
				|| statsCounter->getValue(Stats::blocksMined[Tile::mushroom1_Id], difficulty) > 0
				|| statsCounter->getValue(Stats::blocksMined[Tile::reeds_Id], difficulty) > 0
				|| statsCounter->getValue(Stats::cowsMilked, difficulty) > 0
				|| statsCounter->getValue(Stats::itemsCollected[Tile::pumpkin->id], difficulty) > 0
				|| statsCounter->getValue(Stats::killsZombie, difficulty) > 0
				|| statsCounter->getValue(Stats::killsSkeleton, difficulty) > 0
				|| statsCounter->getValue(Stats::killsCreeper, difficulty) > 0
				|| statsCounter->getValue(Stats::killsSpider, difficulty) > 0
				|| statsCounter->getValue(Stats::killsSpiderJockey, difficulty) > 0
				|| statsCounter->getValue(Stats::killsZombiePigman, difficulty) > 0
				|| statsCounter->getValue(Stats::killsNetherZombiePigman, difficulty) > 0
				|| statsCounter->getValue(Stats::killsSlime, difficulty) > 0)
			{
				return true;
			}
		}

		return false;
	}

	static std::wstring TrimWhitespace(const std::wstring &text)
	{
		size_t start = 0;
		while (start < text.length() && iswspace(text[start]))
		{
			++start;
		}

		size_t end = text.length();
		while (end > start && iswspace(text[end - 1]))
		{
			--end;
		}

		return text.substr(start, end - start);
	}

	static bool IsBooleanText(const std::wstring &text)
	{
		return (_wcsicmp(text.c_str(), L"false") == 0) || (_wcsicmp(text.c_str(), L"true") == 0);
	}

	static std::wstring ToWideName(const char *text)
	{
		if (text == NULL || text[0] == 0)
		{
			return L"";
		}

		wchar_t wideName[XUSER_NAME_SIZE + 1];
		ZeroMemory(wideName, sizeof(wideName));
		MultiByteToWideChar(CP_ACP, 0, text, -1, wideName, XUSER_NAME_SIZE + 1);
		wideName[XUSER_NAME_SIZE] = 0;
		return wideName;
	}

	static int GetLocalPad()
	{
		int iPad = ProfileManager.GetPrimaryPad();
		if (iPad < 0 || iPad >= XUSER_MAX_COUNT)
		{
			iPad = ProfileManager.GetLockedProfile();
		}
		if (iPad < 0 || iPad >= XUSER_MAX_COUNT)
		{
			iPad = 0;
		}
		return iPad;
	}

	static PlayerUID GetLocalPlayerUID(int iPad)
	{
		PlayerUID uid = INVALID_XUID;
		ProfileManager.GetXUID(iPad, &uid, true);
		if (uid == INVALID_XUID)
		{
			ProfileManager.GetXUID(iPad, &uid, false);
		}
		return uid;
	}

	static std::wstring GetLocalDisplayName(int iPad)
	{
		std::wstring displayName = TrimWhitespace(ProfileManager.GetDisplayName(iPad));

		if (displayName.empty() || IsBooleanText(displayName))
		{
			displayName = TrimWhitespace(ToWideName(ProfileManager.GetGamertag(iPad)));
		}

		if (displayName.empty() || IsBooleanText(displayName))
		{
			displayName = L"Player";
		}

		if (displayName.length() > XUSER_NAME_SIZE)
		{
			displayName.resize(XUSER_NAME_SIZE);
		}

		return displayName;
	}

	static StatsCounter *GetLocalStatsCounter(int iPad)
	{
		Minecraft *minecraft = Minecraft::GetInstance();
		if (minecraft == NULL || iPad < 0 || iPad >= XUSER_MAX_COUNT)
		{
			return NULL;
		}

		StatsCounter *statsCounter = minecraft->stats[iPad];
		if (statsCounter == NULL)
		{
			return NULL;
		}

		if (!app.GetGameStarted())
		{
			if (!HasAnyTrackedLeaderboardProgress(statsCounter))
			{
				void *profileData = ProfileManager.GetGameDefinedProfileData(iPad);
				if (profileData != NULL)
				{
					statsCounter->clear();
					statsCounter->parse(profileData);
				}
			}
		}

		return statsCounter;
	}

	static unsigned int GetStatValue(StatsCounter *statsCounter, Stat *stat, int difficulty)
	{
		if (statsCounter == NULL || stat == NULL || difficulty < 0 || difficulty > 3)
		{
			return 0;
		}

		return statsCounter->getValue(stat, (unsigned int)difficulty);
	}

	static unsigned int GetBlockValue(StatsCounter *statsCounter, Tile *tile, int difficulty)
	{
		if (tile == NULL)
		{
			return 0;
		}
		return GetStatValue(statsCounter, Stats::blocksMined[tile->id], difficulty);
	}

	static unsigned int GetCollectedItemValue(StatsCounter *statsCounter, Item *item, int difficulty)
	{
		if (item == NULL)
		{
			return 0;
		}
		return GetStatValue(statsCounter, Stats::itemsCollected[item->id], difficulty);
	}

	static unsigned int GetZombiePigmanKills(StatsCounter *statsCounter, int difficulty)
	{
		unsigned long long total = (unsigned long long)GetStatValue(statsCounter, Stats::killsZombiePigman, difficulty)
			+ (unsigned long long)GetStatValue(statsCounter, Stats::killsNetherZombiePigman, difficulty);
		return (total > 0xFFFFFFFFULL) ? 0xFFFFFFFFU : (unsigned int)total;
	}

	static unsigned short GetStatsSize(LeaderboardManager::EStatsType type)
	{
		switch (type)
		{
		case LeaderboardManager::eStatsType_Travelling:
			return 4;

		case LeaderboardManager::eStatsType_Mining:
			return 7;

		case LeaderboardManager::eStatsType_Farming:
			return 6;

		case LeaderboardManager::eStatsType_Kills:
			return 7;

		default:
			return 0;
		}
	}

	static void SetColumn(LeaderboardManager::ReadScore &score, unsigned int columnIndex, unsigned long value, unsigned long long &total)
	{
		if (columnIndex >= LeaderboardManager::ReadScore::STATSDATA_MAX)
		{
			return;
		}

		score.m_statsData[columnIndex] = value;
		total += value;
	}

	static unsigned long ClampToUnsignedLong(unsigned long long value)
	{
		return (value > 0xFFFFFFFFULL) ? 0xFFFFFFFFUL : (unsigned long)value;
	}

	static const char *GetStatsTypeName(LeaderboardManager::EStatsType type)
	{
		switch (type)
		{
		case LeaderboardManager::eStatsType_Travelling:
			return "Travelling";
		case LeaderboardManager::eStatsType_Mining:
			return "Mining";
		case LeaderboardManager::eStatsType_Farming:
			return "Farming";
		case LeaderboardManager::eStatsType_Kills:
			return "Kills";
		default:
			return "Unknown";
		}
	}

	static void SetTravellingColumns(LeaderboardManager::ReadScore &score, StatsCounter *statsCounter, int difficulty, unsigned long long &total)
	{
		SetColumn(score, 0, GetStatValue(statsCounter, Stats::walkOneM, difficulty), total);
		SetColumn(score, 1, GetStatValue(statsCounter, Stats::fallOneM, difficulty), total);
		SetColumn(score, 2, GetStatValue(statsCounter, Stats::minecartOneM, difficulty), total);
		SetColumn(score, 3, GetStatValue(statsCounter, Stats::boatOneM, difficulty), total);
	}

	static unsigned long long GetTravellingTotalForDifficulty(StatsCounter *statsCounter, int difficulty)
	{
		if (difficulty < 0 || difficulty > 3)
		{
			return 0;
		}

		unsigned long long total = 0;
		total += GetStatValue(statsCounter, Stats::walkOneM, difficulty);
		total += GetStatValue(statsCounter, Stats::fallOneM, difficulty);
		total += GetStatValue(statsCounter, Stats::minecartOneM, difficulty);
		total += GetStatValue(statsCounter, Stats::boatOneM, difficulty);
		return total;
	}

	static void SetMiningColumns(LeaderboardManager::ReadScore &score, StatsCounter *statsCounter, int difficulty, unsigned long long &total)
	{
		SetColumn(score, 0, GetBlockValue(statsCounter, Tile::dirt, difficulty), total);
		SetColumn(score, 1, GetBlockValue(statsCounter, Tile::stoneBrick, difficulty), total);
		SetColumn(score, 2, GetBlockValue(statsCounter, Tile::sand, difficulty), total);
		SetColumn(score, 3, GetBlockValue(statsCounter, Tile::rock, difficulty), total);
		SetColumn(score, 4, GetBlockValue(statsCounter, Tile::gravel, difficulty), total);
		SetColumn(score, 5, GetBlockValue(statsCounter, Tile::clay, difficulty), total);
		SetColumn(score, 6, GetBlockValue(statsCounter, Tile::obsidian, difficulty), total);
	}

	static unsigned long long GetMiningTotalForDifficulty(StatsCounter *statsCounter, int difficulty)
	{
		if (difficulty < 0 || difficulty > 3)
		{
			return 0;
		}

		unsigned long long total = 0;
		total += GetBlockValue(statsCounter, Tile::dirt, difficulty);
		total += GetBlockValue(statsCounter, Tile::stoneBrick, difficulty);
		total += GetBlockValue(statsCounter, Tile::sand, difficulty);
		total += GetBlockValue(statsCounter, Tile::rock, difficulty);
		total += GetBlockValue(statsCounter, Tile::gravel, difficulty);
		total += GetBlockValue(statsCounter, Tile::clay, difficulty);
		total += GetBlockValue(statsCounter, Tile::obsidian, difficulty);
		return total;
	}

	static void SetFarmingColumns(LeaderboardManager::ReadScore &score, StatsCounter *statsCounter, int difficulty, unsigned long long &total)
	{
		SetColumn(score, 0, GetCollectedItemValue(statsCounter, Item::egg, difficulty), total);
		SetColumn(score, 1, GetStatValue(statsCounter, Stats::blocksMined[Tile::crops_Id], difficulty), total);
		SetColumn(score, 2, GetStatValue(statsCounter, Stats::blocksMined[Tile::mushroom1_Id], difficulty), total);
		SetColumn(score, 3, GetStatValue(statsCounter, Stats::blocksMined[Tile::reeds_Id], difficulty), total);
		SetColumn(score, 4, GetStatValue(statsCounter, Stats::cowsMilked, difficulty), total);
		SetColumn(score, 5, GetStatValue(statsCounter, Stats::itemsCollected[Tile::pumpkin->id], difficulty), total);
	}

	static unsigned long long GetFarmingTotalForDifficulty(StatsCounter *statsCounter, int difficulty)
	{
		if (difficulty < 0 || difficulty > 3)
		{
			return 0;
		}

		unsigned long long total = 0;
		total += GetCollectedItemValue(statsCounter, Item::egg, difficulty);
		total += GetStatValue(statsCounter, Stats::blocksMined[Tile::crops_Id], difficulty);
		total += GetStatValue(statsCounter, Stats::blocksMined[Tile::mushroom1_Id], difficulty);
		total += GetStatValue(statsCounter, Stats::blocksMined[Tile::reeds_Id], difficulty);
		total += GetStatValue(statsCounter, Stats::cowsMilked, difficulty);
		total += GetStatValue(statsCounter, Stats::itemsCollected[Tile::pumpkin->id], difficulty);
		return total;
	}

	static void SetKillsColumns(LeaderboardManager::ReadScore &score, StatsCounter *statsCounter, int difficulty, unsigned long long &total)
	{
		SetColumn(score, 0, GetStatValue(statsCounter, Stats::killsZombie, difficulty), total);
		SetColumn(score, 1, GetStatValue(statsCounter, Stats::killsSkeleton, difficulty), total);
		SetColumn(score, 2, GetStatValue(statsCounter, Stats::killsCreeper, difficulty), total);
		SetColumn(score, 3, GetStatValue(statsCounter, Stats::killsSpider, difficulty), total);
		SetColumn(score, 4, GetStatValue(statsCounter, Stats::killsSpiderJockey, difficulty), total);
		SetColumn(score, 5, GetZombiePigmanKills(statsCounter, difficulty), total);
		SetColumn(score, 6, GetStatValue(statsCounter, Stats::killsSlime, difficulty), total);
	}

	static unsigned long long GetKillsTotalForDifficulty(StatsCounter *statsCounter, int difficulty)
	{
		if (difficulty < 0 || difficulty > 3)
		{
			return 0;
		}

		unsigned long long total = 0;
		total += GetStatValue(statsCounter, Stats::killsZombie, difficulty);
		total += GetStatValue(statsCounter, Stats::killsSkeleton, difficulty);
		total += GetStatValue(statsCounter, Stats::killsCreeper, difficulty);
		total += GetStatValue(statsCounter, Stats::killsSpider, difficulty);
		total += GetStatValue(statsCounter, Stats::killsSpiderJockey, difficulty);
		total += GetZombiePigmanKills(statsCounter, difficulty);
		total += GetStatValue(statsCounter, Stats::killsSlime, difficulty);
		return total;
	}

	static bool SetColumnsForType(LeaderboardManager::ReadScore &score, StatsCounter *statsCounter, int difficulty, LeaderboardManager::EStatsType type, unsigned long long &total)
	{
		switch (type)
		{
		case LeaderboardManager::eStatsType_Travelling:
			SetTravellingColumns(score, statsCounter, difficulty, total);
			return true;

		case LeaderboardManager::eStatsType_Mining:
			SetMiningColumns(score, statsCounter, difficulty, total);
			return true;

		case LeaderboardManager::eStatsType_Farming:
			SetFarmingColumns(score, statsCounter, difficulty, total);
			return true;

		case LeaderboardManager::eStatsType_Kills:
			SetKillsColumns(score, statsCounter, difficulty, total);
			return true;

		default:
			return false;
		}
	}

	static unsigned long long GetTypeTotalForDifficulty(StatsCounter *statsCounter, int difficulty, LeaderboardManager::EStatsType type)
	{
		switch (type)
		{
		case LeaderboardManager::eStatsType_Travelling:
			return GetTravellingTotalForDifficulty(statsCounter, difficulty);

		case LeaderboardManager::eStatsType_Mining:
			return GetMiningTotalForDifficulty(statsCounter, difficulty);

		case LeaderboardManager::eStatsType_Farming:
			return GetFarmingTotalForDifficulty(statsCounter, difficulty);

		case LeaderboardManager::eStatsType_Kills:
			return GetKillsTotalForDifficulty(statsCounter, difficulty);

		default:
			return 0;
		}
	}

	static void DebugDumpZeroRead(StatsCounter *statsCounter, int iPad, int selectedDifficulty, LeaderboardManager::EStatsType type, unsigned long long selectedTotal)
	{
		if (selectedTotal != 0)
		{
			return;
		}

		unsigned long long totals[4];
		for (int d = 0; d < 4; ++d)
		{
			totals[d] = GetTypeTotalForDifficulty(statsCounter, d, type);
		}

		app.DebugPrintf(
			"[Win64 LB] %s row is zero (pad=%d selectedDiff=%d canRecord=%d gameStarted=%d) totals[p/e/n/h]=%I64u/%I64u/%I64u/%I64u\n",
			GetStatsTypeName(type),
			iPad,
			selectedDifficulty,
			app.CanRecordStatsAndAchievements() ? 1 : 0,
			app.GetGameStarted() ? 1 : 0,
			totals[0],
			totals[1],
			totals[2],
			totals[3]);
	}

	static bool PopulateLocalReadScore(LeaderboardManager::ReadScore &score, int difficulty, LeaderboardManager::EStatsType type)
	{
		if (difficulty < 0 || difficulty > 3 || type < LeaderboardManager::eStatsType_Travelling || type >= LeaderboardManager::eStatsType_MAX)
		{
			return false;
		}

		const int iPad = GetLocalPad();
		StatsCounter *statsCounter = GetLocalStatsCounter(iPad);

		score.m_uid = GetLocalPlayerUID(iPad);
		score.m_rank = 1;
		score.m_name = GetLocalDisplayName(iPad);
		score.m_totalScore = 0;
		score.m_statsSize = GetStatsSize(type);
		for (unsigned int i = 0; i < LeaderboardManager::ReadScore::STATSDATA_MAX; ++i)
		{
			score.m_statsData[i] = 0;
		}
		score.m_idsErrorMessage = 0;

		unsigned long long totalScore = 0;
		if (!SetColumnsForType(score, statsCounter, difficulty, type, totalScore))
		{
			return false;
		}

		if (totalScore == 0)
		{
			unsigned long long bestTotal = 0;
			int bestDifficulty = difficulty;
			for (int d = 0; d < 4; ++d)
			{
				unsigned long long altTotal = GetTypeTotalForDifficulty(statsCounter, d, type);
				if (altTotal > bestTotal)
				{
					bestTotal = altTotal;
					bestDifficulty = d;
				}
			}

			if (bestTotal > 0 && bestDifficulty != difficulty)
			{
				totalScore = 0;
				SetColumnsForType(score, statsCounter, bestDifficulty, type, totalScore);
				app.DebugPrintf("[Win64 LB] %s fallback used alternate difficulty %d for selected difficulty %d (pad=%d)\n", GetStatsTypeName(type), bestDifficulty, difficulty, iPad);
			}
		}

		DebugDumpZeroRead(statsCounter, iPad, difficulty, type, totalScore);

		score.m_totalScore = ClampToUnsignedLong(totalScore);
		return true;
	}
}

LeaderboardManager *LeaderboardManager::m_instance = new WindowsLeaderboardManager();

void WindowsLeaderboardManager::Tick() {}

bool WindowsLeaderboardManager::OpenSession()
{
	return true;
}

void WindowsLeaderboardManager::CloseSession() {}

void WindowsLeaderboardManager::DeleteSession() {}

bool WindowsLeaderboardManager::WriteStats(unsigned int viewCount, ViewIn views)
{
	UNREFERENCED_PARAMETER(viewCount);

	if (views != NULL)
	{
		delete [] views;
	}

	return true;
}

bool WindowsLeaderboardManager::ReadStats_Friends(LeaderboardReadListener *callback, int difficulty, EStatsType type, PlayerUID myUID, unsigned int startIndex, unsigned int readCount)
{
	if (!LeaderboardManager::ReadStats_Friends(callback, difficulty, type, myUID, startIndex, readCount))
	{
		return false;
	}

	return ReadStats_Local(callback, difficulty, type, startIndex, readCount);
}

bool WindowsLeaderboardManager::ReadStats_MyScore(LeaderboardReadListener *callback, int difficulty, EStatsType type, PlayerUID myUID, unsigned int readCount)
{
	if (!LeaderboardManager::ReadStats_MyScore(callback, difficulty, type, myUID, readCount))
	{
		return false;
	}

	return ReadStats_Local(callback, difficulty, type, 1, readCount);
}

bool WindowsLeaderboardManager::ReadStats_TopRank(LeaderboardReadListener *callback, int difficulty, EStatsType type, unsigned int startIndex, unsigned int readCount)
{
	if (!LeaderboardManager::ReadStats_TopRank(callback, difficulty, type, startIndex, readCount))
	{
		return false;
	}

	return ReadStats_Local(callback, difficulty, type, startIndex, readCount);
}

void WindowsLeaderboardManager::FlushStats() {}

void WindowsLeaderboardManager::CancelOperation()
{
	zeroReadParameters();
	m_readListener = NULL;
}

bool WindowsLeaderboardManager::isIdle()
{
	return true;
}

bool WindowsLeaderboardManager::ReadStats_Local(LeaderboardReadListener *listener, int difficulty, EStatsType type, unsigned int startIndex, unsigned int readCount)
{
	UNREFERENCED_PARAMETER(readCount);

	if (listener == NULL)
	{
		return false;
	}

	ViewOut view;
	view.m_numQueries = 0;
	view.m_queries = NULL;

	ReadScore localScore;

	if (startIndex <= 1 && PopulateLocalReadScore(localScore, difficulty, type))
	{
		view.m_numQueries = 1;
		view.m_queries = &localScore;
		listener->OnStatsReadComplete(eStatsReturn_Success, 1, view);
		return true;
	}

	if (difficulty >= 0 && difficulty <= 3 && type >= eStatsType_Travelling && type < eStatsType_MAX)
	{
		listener->OnStatsReadComplete(eStatsReturn_Success, 1, view);
		return true;
	}

	listener->OnStatsReadComplete(eStatsReturn_NoResults, 0, view);
	return false;
}