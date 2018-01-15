/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "DBCStores.h"
#include "DBCfmt.h"
#include "GridMap.h"
#include "logging.h"
#include "ObjectGuid.h"
#include "ProgressBar.h"
#include "SharedDefines.h"
#include "TransportMgr.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"
#include <map>

typedef std::map<uint16, uint32> AreaFlagByAreaID;
typedef std::map<uint32, uint32> AreaFlagByMapID;

struct WMOAreaTableTripple
{
    WMOAreaTableTripple(int32 r, int32 a, int32 g)
      : groupId(g), rootId(r), adtId(a)
    {
    }

    bool operator<(const WMOAreaTableTripple& b) const
    {
        return memcmp(this, &b, sizeof(WMOAreaTableTripple)) < 0;
    }

    // ordered by entropy; that way memcmp will have a minimal medium runtime
    int32 groupId;
    int32 rootId;
    int32 adtId;
};

typedef std::map<WMOAreaTableTripple, WMOAreaTableEntry const*>
    WMOAreaInfoByTripple;

DBCStorage<AreaTableEntry> sAreaStore(AreaTableEntryfmt);
static AreaFlagByAreaID sAreaFlagByAreaID;
static AreaFlagByMapID
    sAreaFlagByMapID; // for instances without generated *.map files

static WMOAreaInfoByTripple sWMOAreaInfoByTripple;

DBCStorage<AreaTriggerEntry> sAreaTriggerStore(AreaTriggerEntryfmt);
DBCStorage<AuctionHouseEntry> sAuctionHouseStore(AuctionHouseEntryfmt);
DBCStorage<BankBagSlotPricesEntry> sBankBagSlotPricesStore(
    BankBagSlotPricesEntryfmt);
DBCStorage<BattlemasterListEntry> sBattlemasterListStore(
    BattlemasterListEntryfmt);
DBCStorage<CharStartOutfitEntry> sCharStartOutfitStore(CharStartOutfitEntryfmt);
DBCStorage<CharTitlesEntry> sCharTitlesStore(CharTitlesEntryfmt);
DBCStorage<ChatChannelsEntry> sChatChannelsStore(ChatChannelsEntryfmt);
DBCStorage<ChrClassesEntry> sChrClassesStore(ChrClassesEntryfmt);
DBCStorage<ChrRacesEntry> sChrRacesStore(ChrRacesEntryfmt);
DBCStorage<CinematicSequencesEntry> sCinematicSequencesStore(
    CinematicSequencesEntryfmt);
DBCStorage<CreatureDisplayInfoEntry> sCreatureDisplayInfoStore(
    CreatureDisplayInfofmt);
DBCStorage<CreatureDisplayInfoExtraEntry> sCreatureDisplayInfoExtraStore(
    CreatureDisplayInfoExtrafmt);
DBCStorage<CreatureFamilyEntry> sCreatureFamilyStore(CreatureFamilyfmt);
DBCStorage<CreatureSpellDataEntry> sCreatureSpellDataStore(
    CreatureSpellDatafmt);
DBCStorage<CreatureTypeEntry> sCreatureTypeStore(CreatureTypefmt);

DBCStorage<DurabilityQualityEntry> sDurabilityQualityStore(
    DurabilityQualityfmt);
DBCStorage<DurabilityCostsEntry> sDurabilityCostsStore(DurabilityCostsfmt);

DBCStorage<EmotesEntry> sEmotesStore(EmotesEntryfmt);
DBCStorage<EmotesTextEntry> sEmotesTextStore(EmotesTextEntryfmt);

typedef std::map<uint32, SimpleFactionsList> FactionTeamMap;
static FactionTeamMap sFactionTeamMap;
DBCStorage<FactionEntry> sFactionStore(FactionEntryfmt);
DBCStorage<FactionTemplateEntry> sFactionTemplateStore(FactionTemplateEntryfmt);

DBCStorage<GameObjectDisplayInfoEntry> sGameObjectDisplayInfoStore(
    GameObjectDisplayInfofmt);
DBCStorage<GemPropertiesEntry> sGemPropertiesStore(GemPropertiesEntryfmt);

DBCStorage<GtCombatRatingsEntry> sGtCombatRatingsStore(GtCombatRatingsfmt);
DBCStorage<GtChanceToMeleeCritBaseEntry> sGtChanceToMeleeCritBaseStore(
    GtChanceToMeleeCritBasefmt);
DBCStorage<GtChanceToMeleeCritEntry> sGtChanceToMeleeCritStore(
    GtChanceToMeleeCritfmt);
DBCStorage<GtChanceToSpellCritBaseEntry> sGtChanceToSpellCritBaseStore(
    GtChanceToSpellCritBasefmt);
DBCStorage<GtChanceToSpellCritEntry> sGtChanceToSpellCritStore(
    GtChanceToSpellCritfmt);
DBCStorage<GtOCTRegenHPEntry> sGtOCTRegenHPStore(GtOCTRegenHPfmt);
// DBCStorage <GtOCTRegenMPEntry>
// sGtOCTRegenMPStore(GtOCTRegenMPfmt);  -- not used currently
DBCStorage<GtRegenHPPerSptEntry> sGtRegenHPPerSptStore(GtRegenHPPerSptfmt);
DBCStorage<GtRegenMPPerSptEntry> sGtRegenMPPerSptStore(GtRegenMPPerSptfmt);

DBCStorage<ItemEntry> sItemStore(Itemfmt);
DBCStorage<ItemBagFamilyEntry> sItemBagFamilyStore(ItemBagFamilyfmt);
DBCStorage<ItemClassEntry> sItemClassStore(ItemClassfmt);
// DBCStorage <ItemCondExtCostsEntry>
// sItemCondExtCostsStore(ItemCondExtCostsEntryfmt);
// DBCStorage <ItemDisplayInfoEntry>
// sItemDisplayInfoStore(ItemDisplayTemplateEntryfmt); -- not used currently
DBCStorage<ItemExtendedCostEntry> sItemExtendedCostStore(
    ItemExtendedCostEntryfmt);
DBCStorage<ItemRandomPropertiesEntry> sItemRandomPropertiesStore(
    ItemRandomPropertiesfmt);
DBCStorage<ItemRandomSuffixEntry> sItemRandomSuffixStore(ItemRandomSuffixfmt);
DBCStorage<ItemSetEntry> sItemSetStore(ItemSetEntryfmt);

DBCStorage<LiquidTypeEntry> sLiquidTypeStore(LiquidTypefmt);
DBCStorage<LockEntry> sLockStore(LockEntryfmt);

DBCStorage<MailTemplateEntry> sMailTemplateStore(MailTemplateEntryfmt);
DBCStorage<MapEntry> sMapStore(MapEntryfmt);

DBCStorage<QuestSortEntry> sQuestSortStore(QuestSortEntryfmt);

DBCStorage<RandomPropertiesPointsEntry> sRandomPropertiesPointsStore(
    RandomPropertiesPointsfmt);

DBCStorage<SkillLineEntry> sSkillLineStore(SkillLinefmt);
DBCStorage<SkillLineAbilityEntry> sSkillLineAbilityStore(SkillLineAbilityfmt);
DBCStorage<SkillRaceClassInfoEntry> sSkillRaceClassInfoStore(
    SkillRaceClassInfofmt);

DBCStorage<SoundEntriesEntry> sSoundEntriesStore(SoundEntriesfmt);

DBCStorage<SpellItemEnchantmentEntry> sSpellItemEnchantmentStore(
    SpellItemEnchantmentfmt);
DBCStorage<SpellItemEnchantmentConditionEntry>
    sSpellItemEnchantmentConditionStore(SpellItemEnchantmentConditionfmt);
DBCStorage<SpellEntry> sSpellStore(SpellEntryfmt);
SpellCategoryStore sSpellCategoryStore;
PetFamilySpellsStore sPetFamilySpellsStore;

DBCStorage<SpellCastTimesEntry> sSpellCastTimesStore(SpellCastTimefmt);
DBCStorage<SpellDurationEntry> sSpellDurationStore(SpellDurationfmt);
DBCStorage<SpellFocusObjectEntry> sSpellFocusObjectStore(SpellFocusObjectfmt);
DBCStorage<SpellRadiusEntry> sSpellRadiusStore(SpellRadiusfmt);
DBCStorage<SpellRangeEntry> sSpellRangeStore(SpellRangefmt);
DBCStorage<SpellShapeshiftFormEntry> sSpellShapeshiftFormStore(
    SpellShapeshiftfmt);
DBCStorage<StableSlotPricesEntry> sStableSlotPricesStore(StableSlotPricesfmt);
DBCStorage<SummonPropertiesEntry> sSummonPropertiesStore(SummonPropertiesfmt);
DBCStorage<TalentEntry> sTalentStore(TalentEntryfmt);
TalentSpellPosMap sTalentSpellPosMap;
DBCStorage<TalentTabEntry> sTalentTabStore(TalentTabEntryfmt);

// store absolute bit position for first rank for talent inspect
typedef std::map<uint32, uint32> TalentInspectMap;
static TalentInspectMap sTalentPosInInspect;
static TalentInspectMap sTalentTabSizeInInspect;
static uint32 sTalentTabPages[12 /*MAX_CLASSES*/][3];

DBCStorage<TaxiNodesEntry> sTaxiNodesStore(TaxiNodesEntryfmt);
TaxiMask sTaxiNodesMask;

// DBC used only for initialization sTaxiPathSetBySource at startup.
TaxiPathSetBySource sTaxiPathSetBySource;
DBCStorage<TaxiPathEntry> sTaxiPathStore(TaxiPathEntryfmt);

// DBC store data but sTaxiPathNodesByPath used for fast access to entries (it's
// not owner pointed data).
TaxiPathNodesByPath sTaxiPathNodesByPath;
static DBCStorage<TaxiPathNodeEntry> sTaxiPathNodeStore(TaxiPathNodeEntryfmt);

DBCStorage<TotemCategoryEntry> sTotemCategoryStore(TotemCategoryEntryfmt);
DBCStorage<TransportAnimationEntry> sTransportAnimationStore(
    TransportAnimationfmt);
DBCStorage<WMOAreaTableEntry> sWMOAreaTableStore(WMOAreaTableEntryfmt);
DBCStorage<WorldMapAreaEntry> sWorldMapAreaStore(WorldMapAreaEntryfmt);
// DBCStorage <WorldMapOverlayEntry>
// sWorldMapOverlayStore(WorldMapOverlayEntryfmt);
DBCStorage<WorldSafeLocsEntry> sWorldSafeLocsStore(WorldSafeLocsEntryfmt);

typedef std::list<std::string> StoreProblemList;

bool IsAcceptableClientBuild(uint32 build)
{
    int accepted_versions[] = EXPECTED_MANGOSD_CLIENT_BUILD;
    for (int i = 0; accepted_versions[i]; ++i)
        if (int(build) == accepted_versions[i])
            return true;

    return false;
}

std::string AcceptableClientBuildsListStr()
{
    std::ostringstream data;
    int accepted_versions[] = EXPECTED_MANGOSD_CLIENT_BUILD;
    for (int i = 0; accepted_versions[i]; ++i)
        data << accepted_versions[i] << " ";
    return data.str();
}

#ifndef NDEBUG
static bool LoadDBC_assert_print(
    uint32 fsize, uint32 rsize, const std::string& filename)
{
    logging.error(
        "Size of '%s' setted by format string (%u) not equal size of C++ "
        "structure (%u).",
        filename.c_str(), fsize, rsize);

    // ASSERT must fail after function call
    return false;
}
#endif

static int DBCLoadedCount = 0;

template <class T>
inline void LoadDBC(uint32& availableDbcLocales, BarGoLink& bar,
    StoreProblemList& errlist, DBCStorage<T>& storage,
    const std::string& dbc_path, const std::string& filename)
{
    // compatibility format and C++ structure sizes
    assert(
        DBCFileLoader::GetFormatRecordSize(storage.GetFormat()) == sizeof(T) ||
        LoadDBC_assert_print(
            DBCFileLoader::GetFormatRecordSize(storage.GetFormat()), sizeof(T),
            filename));

    std::string dbc_filename = dbc_path + filename;
    if (storage.Load(dbc_filename.c_str()))
    {
        bar.step();
        ++DBCLoadedCount;
        for (uint8 i = 0; fullLocaleNameList[i].name; ++i)
        {
            if (!(availableDbcLocales & (1 << i)))
                continue;

            std::string dbc_filename_loc =
                dbc_path + fullLocaleNameList[i].name + "/" + filename;
            if (!storage.LoadStringsFrom(dbc_filename_loc.c_str()))
                availableDbcLocales &=
                    ~(1 << i); // mark as not available for speedup next checks
        }
    }
    else
    {
        // sort problematic dbc to (1) non compatible and (2) nonexistent
        FILE* f = fopen(dbc_filename.c_str(), "rb");
        if (f)
        {
            char buf[100];
            snprintf(buf, 100, " (exist, but have %u fields instead " SIZEFMTD
                               ") Wrong client version DBC file?",
                storage.GetFieldCount(), strlen(storage.GetFormat()));
            errlist.push_back(dbc_filename + buf);
            fclose(f);
        }
        else
            errlist.push_back(dbc_filename);
    }
}

void LoadDBCStores(const std::string& dataPath)
{
    std::string dbcPath = dataPath + "dbc/";

    const uint32 DBCFilesCount = 69;

    BarGoLink bar(DBCFilesCount);

    StoreProblemList bad_dbc_files;

    // bitmask for index of fullLocaleNameList
    uint32 availableDbcLocales = 0xFFFFFFFF;

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sAreaStore, dbcPath,
        "AreaTable.dbc");

    // must be after sAreaStore loading
    for (uint32 i = 0; i < sAreaStore.GetNumRows();
         ++i) // areaflag numbered from 0
    {
        if (AreaTableEntry const* area = sAreaStore.LookupEntry(i))
        {
            // fill AreaId->DBC records
            sAreaFlagByAreaID.insert(AreaFlagByAreaID::value_type(
                uint16(area->ID), area->exploreFlag));

            // fill MapId->DBC records ( skip sub zones and continents )
            if (area->zone == 0 && area->mapid != 0 && area->mapid != 1 &&
                area->mapid != 530)
                sAreaFlagByMapID.insert(AreaFlagByMapID::value_type(
                    area->mapid, area->exploreFlag));
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sAreaTriggerStore, dbcPath,
        "AreaTrigger.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sAuctionHouseStore,
        dbcPath, "AuctionHouse.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sBankBagSlotPricesStore,
        dbcPath, "BankBagSlotPrices.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sBattlemasterListStore,
        dbcPath, "BattlemasterList.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sCharStartOutfitStore,
        dbcPath, "CharStartOutfit.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sCharTitlesStore, dbcPath,
        "CharTitles.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sChatChannelsStore,
        dbcPath, "ChatChannels.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sChrClassesStore, dbcPath,
        "ChrClasses.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sChrRacesStore, dbcPath,
        "ChrRaces.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sCinematicSequencesStore,
        dbcPath, "CinematicSequences.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sCreatureDisplayInfoStore,
        dbcPath, "CreatureDisplayInfo.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files,
        sCreatureDisplayInfoExtraStore, dbcPath,
        "CreatureDisplayInfoExtra.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sCreatureFamilyStore,
        dbcPath, "CreatureFamily.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sCreatureSpellDataStore,
        dbcPath, "CreatureSpellData.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sCreatureTypeStore,
        dbcPath, "CreatureType.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sDurabilityCostsStore,
        dbcPath, "DurabilityCosts.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sDurabilityQualityStore,
        dbcPath, "DurabilityQuality.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sEmotesStore, dbcPath,
        "Emotes.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sEmotesTextStore, dbcPath,
        "EmotesText.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sFactionStore, dbcPath,
        "Faction.dbc");
    for (uint32 i = 0; i < sFactionStore.GetNumRows(); ++i)
    {
        FactionEntry const* faction = sFactionStore.LookupEntry(i);
        if (faction && faction->team)
        {
            SimpleFactionsList& flist = sFactionTeamMap[faction->team];
            flist.push_back(i);
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sFactionTemplateStore,
        dbcPath, "FactionTemplate.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files,
        sGameObjectDisplayInfoStore, dbcPath, "GameObjectDisplayInfo.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGemPropertiesStore,
        dbcPath, "GemProperties.dbc");

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGtCombatRatingsStore,
        dbcPath, "gtCombatRatings.dbc");

    LoadDBC(availableDbcLocales, bar, bad_dbc_files,
        sGtChanceToMeleeCritBaseStore, dbcPath, "gtChanceToMeleeCritBase.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGtChanceToMeleeCritStore,
        dbcPath, "gtChanceToMeleeCrit.dbc");

    LoadDBC(availableDbcLocales, bar, bad_dbc_files,
        sGtChanceToSpellCritBaseStore, dbcPath, "gtChanceToSpellCritBase.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGtChanceToSpellCritStore,
        dbcPath, "gtChanceToSpellCrit.dbc");

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGtOCTRegenHPStore,
        dbcPath, "gtOCTRegenHP.dbc");
    // LoadDBC(availableDbcLocales,bar,bad_dbc_files,sGtOCTRegenMPStore,
    // dbcPath,"gtOCTRegenMP.dbc");       -- not used currently
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGtRegenHPPerSptStore,
        dbcPath, "gtRegenHPPerSpt.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGtRegenMPPerSptStore,
        dbcPath, "gtRegenMPPerSpt.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sItemStore, dbcPath,
        "Item.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sItemBagFamilyStore,
        dbcPath, "ItemBagFamily.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sItemClassStore, dbcPath,
        "ItemClass.dbc");
    // LoadDBC(availableDbcLocales,bar,bad_dbc_files,sItemDisplayInfoStore,
    // dbcPath,"ItemDisplayInfo.dbc");     -- not used currently
    // LoadDBC(availableDbcLocales,bar,bad_dbc_files,sItemCondExtCostsStore,
    // dbcPath,"ItemCondExtCosts.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sItemExtendedCostStore,
        dbcPath, "ItemExtendedCost.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sItemRandomPropertiesStore,
        dbcPath, "ItemRandomProperties.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sItemRandomSuffixStore,
        dbcPath, "ItemRandomSuffix.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sItemSetStore, dbcPath,
        "ItemSet.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sLiquidTypeStore, dbcPath,
        "LiquidType.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sLockStore, dbcPath,
        "Lock.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sMailTemplateStore,
        dbcPath, "MailTemplate.dbc");
    LoadDBC(
        availableDbcLocales, bar, bad_dbc_files, sMapStore, dbcPath, "Map.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sQuestSortStore, dbcPath,
        "QuestSort.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files,
        sRandomPropertiesPointsStore, dbcPath, "RandPropPoints.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSkillLineStore, dbcPath,
        "SkillLine.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSkillLineAbilityStore,
        dbcPath, "SkillLineAbility.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSkillRaceClassInfoStore,
        dbcPath, "SkillRaceClassInfo.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSoundEntriesStore,
        dbcPath, "SoundEntries.dbc");
    LoadSpellStoreFromDB(sSpellStore);
    for (uint32 i = 1; i < sSpellStore.GetNumRows(); ++i)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(i);
        if (spell && spell->Category)
            sSpellCategoryStore[spell->Category].insert(i);

// DBC not support uint64 fields but SpellEntry have SpellFamilyFlags mapped at
// 2 uint32 fields
// uint32 field already converted to bigendian if need, but must be swapped for
// correct uint64 bigendian view
#if MANGOS_ENDIAN == MANGOS_BIGENDIAN
        std::swap(*((uint32*)(&spell->SpellFamilyFlags)),
            *(((uint32*)(&spell->SpellFamilyFlags)) + 1));
#endif
    }

    for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
    {
        SkillLineAbilityEntry const* skillLine =
            sSkillLineAbilityStore.LookupEntry(j);

        if (!skillLine)
            continue;

        SpellEntry const* spellInfo =
            sSpellStore.LookupEntry(skillLine->spellId);
        if (spellInfo &&
            (spellInfo->Attributes & (SPELL_ATTR_ABILITY | SPELL_ATTR_PASSIVE |
                                         SPELL_ATTR_UNK7 | SPELL_ATTR_UNK8)) ==
                (SPELL_ATTR_ABILITY | SPELL_ATTR_PASSIVE | SPELL_ATTR_UNK7 |
                    SPELL_ATTR_UNK8))
        {
            for (unsigned int i = 1; i < sCreatureFamilyStore.GetNumRows(); ++i)
            {
                CreatureFamilyEntry const* cFamily =
                    sCreatureFamilyStore.LookupEntry(i);
                if (!cFamily)
                    continue;

                if (skillLine->skillId != cFamily->skillLine[0] &&
                    skillLine->skillId != cFamily->skillLine[1])
                    continue;

                sPetFamilySpellsStore[i].insert(spellInfo->Id);
            }
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellCastTimesStore,
        dbcPath, "SpellCastTimes.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellDurationStore,
        dbcPath, "SpellDuration.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellFocusObjectStore,
        dbcPath, "SpellFocusObject.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellItemEnchantmentStore,
        dbcPath, "SpellItemEnchantment.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files,
        sSpellItemEnchantmentConditionStore, dbcPath,
        "SpellItemEnchantmentCondition.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellRadiusStore, dbcPath,
        "SpellRadius.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellRangeStore, dbcPath,
        "SpellRange.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellShapeshiftFormStore,
        dbcPath, "SpellShapeshiftForm.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sStableSlotPricesStore,
        dbcPath, "StableSlotPrices.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSummonPropertiesStore,
        dbcPath, "SummonProperties.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTalentStore, dbcPath,
        "Talent.dbc");

    // create talent spells set
    for (unsigned int i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
        if (!talentInfo)
            continue;
        for (int j = 0; j < 5; j++)
            if (talentInfo->RankID[j])
                sTalentSpellPosMap[talentInfo->RankID[j]] =
                    TalentSpellPos(i, j);
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTalentTabStore, dbcPath,
        "TalentTab.dbc");

    // prepare fast data access to bit pos of talent ranks for use at inspecting
    {
        // fill table by amount of talent ranks and fill
        // sTalentTabBitSizeInInspect
        // store in with (row,col,talent)->size key for correct sorting by
        // (row,col)
        typedef std::map<uint32, uint32> TalentBitSize;
        TalentBitSize sTalentBitSize;
        for (uint32 i = 1; i < sTalentStore.GetNumRows(); ++i)
        {
            TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
            if (!talentInfo)
                continue;

            TalentTabEntry const* talentTabInfo =
                sTalentTabStore.LookupEntry(talentInfo->TalentTab);
            if (!talentTabInfo)
                continue;

            // find talent rank
            uint32 curtalent_maxrank = 0;
            for (uint32 k = 5; k > 0; --k)
            {
                if (talentInfo->RankID[k - 1])
                {
                    curtalent_maxrank = k;
                    break;
                }
            }

            sTalentBitSize[(talentInfo->Row << 24) + (talentInfo->Col << 16) +
                           talentInfo->TalentID] = curtalent_maxrank;
            sTalentTabSizeInInspect[talentInfo->TalentTab] += curtalent_maxrank;
        }

        // now have all max ranks (and then bit amount used for store talent
        // ranks in inspect)
        for (uint32 talentTabId = 1; talentTabId < sTalentTabStore.GetNumRows();
             ++talentTabId)
        {
            TalentTabEntry const* talentTabInfo =
                sTalentTabStore.LookupEntry(talentTabId);
            if (!talentTabInfo)
                continue;

            // prevent memory corruption; otherwise cls will become 12 below
            if ((talentTabInfo->ClassMask & CLASSMASK_ALL_PLAYABLE) == 0)
                continue;

            // store class talent tab pages
            uint32 cls = 1;
            for (uint32 m = 1;
                 !(m & talentTabInfo->ClassMask) && cls < MAX_CLASSES;
                 m <<= 1, ++cls)
            {
            }

            sTalentTabPages[cls][talentTabInfo->tabpage] = talentTabId;

            // add total amount bits for first rank starting from talent tab
            // first talent rank pos.
            uint32 pos = 0;
            for (auto& elem : sTalentBitSize)
            {
                uint32 talentId = elem.first & 0xFFFF;
                TalentEntry const* talentInfo =
                    sTalentStore.LookupEntry(talentId);
                if (!talentInfo)
                    continue;

                if (talentInfo->TalentTab != talentTabId)
                    continue;

                sTalentPosInInspect[talentId] = pos;
                pos += elem.second;
            }
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTaxiNodesStore, dbcPath,
        "TaxiNodes.dbc");

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTaxiPathStore, dbcPath,
        "TaxiPath.dbc");
    for (uint32 i = 1; i < sTaxiPathStore.GetNumRows(); ++i)
        if (TaxiPathEntry const* entry = sTaxiPathStore.LookupEntry(i))
            sTaxiPathSetBySource[entry->from][entry->to] =
                TaxiPathBySourceAndDestination(entry->ID, entry->price);
    uint32 pathCount = sTaxiPathStore.GetNumRows();

    //## TaxiPathNode.dbc ## Loaded only for initialization different structures
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTaxiPathNodeStore,
        dbcPath, "TaxiPathNode.dbc");
    // Calculate path nodes count
    std::vector<uint32> pathLength;
    pathLength.resize(pathCount); // 0 and some other indexes not used
    for (uint32 i = 1; i < sTaxiPathNodeStore.GetNumRows(); ++i)
        if (TaxiPathNodeEntry const* entry = sTaxiPathNodeStore.LookupEntry(i))
        {
            if (pathLength[entry->path] < entry->index + 1)
                pathLength[entry->path] = entry->index + 1;
        }
    // Set path length
    sTaxiPathNodesByPath.resize(pathCount); // 0 and some other indexes not used
    for (uint32 i = 1; i < sTaxiPathNodesByPath.size(); ++i)
        sTaxiPathNodesByPath[i].resize(pathLength[i]);
    // fill data (pointers to sTaxiPathNodeStore elements
    for (uint32 i = 1; i < sTaxiPathNodeStore.GetNumRows(); ++i)
        if (TaxiPathNodeEntry const* entry = sTaxiPathNodeStore.LookupEntry(i))
            sTaxiPathNodesByPath[entry->path].set(entry->index, entry);

    // Initialize global taxinodes mask
    // include existing nodes that have at least single not spell base
    // (scripted) path
    {
        std::set<uint32> spellPaths;
        for (uint32 i = 1; i < sSpellStore.GetNumRows(); ++i)
            if (SpellEntry const* sInfo = sSpellStore.LookupEntry(i))
                for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
                    if (sInfo->Effect[j] == 123 /*SPELL_EFFECT_SEND_TAXI*/)
                        spellPaths.insert(sInfo->EffectMiscValue[j]);

        memset(sTaxiNodesMask, 0, sizeof(sTaxiNodesMask));
        for (uint32 i = 1; i < sTaxiNodesStore.GetNumRows(); ++i)
        {
            TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(i);
            if (!node)
                continue;

            TaxiPathSetBySource::const_iterator src_i =
                sTaxiPathSetBySource.find(i);
            if (src_i != sTaxiPathSetBySource.end() && !src_i->second.empty())
            {
                bool ok = false;
                for (const auto& elem : src_i->second)
                {
                    // not spell path
                    if (spellPaths.find(elem.second.ID) == spellPaths.end())
                    {
                        ok = true;
                        break;
                    }
                }

                if (!ok)
                    continue;
            }

            // valid taxi network node
            uint8 field = (uint8)((i - 1) / 32);
            uint32 submask = 1 << ((i - 1) % 32);
            sTaxiNodesMask[field] |= submask;
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTotemCategoryStore,
        dbcPath, "TotemCategory.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTransportAnimationStore,
        dbcPath, "TransportAnimation.dbc");
    for (uint32 i = 0; i < sTransportAnimationStore.GetNumRows(); ++i)
    {
        TransportAnimationEntry const* anim =
            sTransportAnimationStore.LookupEntry(i);
        if (!anim)
            continue;

        sTransportMgr::Instance()->AddPathNodeToTransport(
            anim->TransportEntry, anim->TimeSeg, anim);
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sWorldMapAreaStore,
        dbcPath, "WorldMapArea.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sWMOAreaTableStore,
        dbcPath, "WMOAreaTable.dbc");
    for (uint32 i = 0; i < sWMOAreaTableStore.GetNumRows(); ++i)
    {
        if (WMOAreaTableEntry const* entry = sWMOAreaTableStore.LookupEntry(i))
        {
            sWMOAreaInfoByTripple.insert(WMOAreaInfoByTripple::value_type(
                WMOAreaTableTripple(
                    entry->rootId, entry->adtId, entry->groupId),
                entry));
        }
    }
    // LoadDBC(availableDbcLocales,bar,bad_dbc_files,sWorldMapOverlayStore,
    // dbcPath,"WorldMapOverlay.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sWorldSafeLocsStore,
        dbcPath, "WorldSafeLocs.dbc");

    // error checks
    if (bad_dbc_files.size() >= DBCFilesCount)
    {
        logging.error(
            "\nIncorrect DataDir value in mangosd.conf or ALL required *.dbc "
            "files (%d) not found by path: %sdbc",
            DBCFilesCount, dataPath.c_str());

        exit(1);
    }
    else if (!bad_dbc_files.empty())
    {
        std::string str;
        for (auto& bad_dbc_file : bad_dbc_files)
            str += bad_dbc_file + "\n";

        logging.error(
            "\nSome required *.dbc files (%u from %d) not found or not "
            "compatible:\n%s",
            (uint32)bad_dbc_files.size(), DBCFilesCount, str.c_str());

        exit(1);
    }

    // Check loaded DBC files proper version
    if (!sSpellStore.LookupEntry(53085) ||
        !sSkillLineAbilityStore.LookupEntry(17514) ||
        !sMapStore.LookupEntry(598) || !sGemPropertiesStore.LookupEntry(1127) ||
        !sItemExtendedCostStore.LookupEntry(2425) ||
        !sCharTitlesStore.LookupEntry(71) || !sAreaStore.LookupEntry(1768))
    {
        logging.error(
            "\nYou have _outdated_ DBC files. Please re-extract DBC files for "
            "one from client build: %s",
            AcceptableClientBuildsListStr().c_str());

        exit(1);
    }

    logging.info("Initialized %d data stores (expected %d)\n", DBCLoadedCount,
        DBCFilesCount);
}

SimpleFactionsList const* GetFactionTeamList(uint32 faction)
{
    FactionTeamMap::const_iterator itr = sFactionTeamMap.find(faction);
    if (itr == sFactionTeamMap.end())
        return nullptr;
    return &itr->second;
}

char const* GetPetName(uint32 petfamily, uint32 dbclang)
{
    if (!petfamily)
        return nullptr;
    CreatureFamilyEntry const* pet_family =
        sCreatureFamilyStore.LookupEntry(petfamily);
    if (!pet_family)
        return nullptr;
    return pet_family->Name[dbclang] ? pet_family->Name[dbclang] : nullptr;
}

TalentSpellPos const* GetTalentSpellPos(uint32 spellId)
{
    TalentSpellPosMap::const_iterator itr = sTalentSpellPosMap.find(spellId);
    if (itr == sTalentSpellPosMap.end())
        return nullptr;

    return &itr->second;
}

uint32 GetTalentSpellCost(TalentSpellPos const* pos)
{
    if (pos)
        return pos->rank + 1;

    return 0;
}

uint32 GetTalentSpellCost(uint32 spellId)
{
    return GetTalentSpellCost(GetTalentSpellPos(spellId));
}

int32 GetAreaFlagByAreaID(uint32 area_id)
{
    auto i = sAreaFlagByAreaID.find(area_id);
    if (i == sAreaFlagByAreaID.end())
        return -1;

    return i->second;
}

WMOAreaTableEntry const* GetWMOAreaTableEntryByTripple(
    int32 rootid, int32 adtid, int32 groupid)
{
    auto itr =
        sWMOAreaInfoByTripple.find(WMOAreaTableTripple(rootid, adtid, groupid));
    if (itr == sWMOAreaInfoByTripple.end())
        return nullptr;
    return itr->second;
}

AreaTableEntry const* GetAreaEntryByAreaID(uint32 area_id)
{
    int32 areaflag = GetAreaFlagByAreaID(area_id);
    if (areaflag < 0)
        return nullptr;

    return sAreaStore.LookupEntry(areaflag);
}

AreaTableEntry const* GetAreaEntryByAreaFlagAndMap(
    uint32 area_flag, uint32 map_id)
{
    if (area_flag)
        return sAreaStore.LookupEntry(area_flag);

    if (MapEntry const* mapEntry = sMapStore.LookupEntry(map_id))
        return GetAreaEntryByAreaID(mapEntry->linked_zone);

    return nullptr;
}

uint32 GetAreaFlagByMapId(uint32 mapid)
{
    auto i = sAreaFlagByMapID.find(mapid);
    if (i == sAreaFlagByMapID.end())
        return 0;
    else
        return i->second;
}

uint32 GetVirtualMapForMapAndZone(uint32 mapid, uint32 zoneId)
{
    if (mapid != 530) // speed for most cases
        return mapid;

    if (WorldMapAreaEntry const* wma = sWorldMapAreaStore.LookupEntry(zoneId))
        return wma->virtual_map_id >= 0 ? wma->virtual_map_id : wma->map_id;

    return mapid;
}

ContentLevels GetContentLevelsForMapAndZone(uint32 mapid, uint32 zoneId)
{
    mapid = GetVirtualMapForMapAndZone(mapid, zoneId);
    if (mapid < 2)
        return CONTENT_1_60;

    MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);
    if (!mapEntry)
        return CONTENT_1_60;

    switch (mapEntry->Expansion())
    {
    default:
        return CONTENT_1_60;
    case 1:
        return CONTENT_61_70;
    }
}

bool IsTotemCategoryCompatiableWith(
    uint32 itemTotemCategoryId, uint32 requiredTotemCategoryId)
{
    if (requiredTotemCategoryId == 0)
        return true;
    if (itemTotemCategoryId == 0)
        return false;

    TotemCategoryEntry const* itemEntry =
        sTotemCategoryStore.LookupEntry(itemTotemCategoryId);
    if (!itemEntry)
        return false;
    TotemCategoryEntry const* reqEntry =
        sTotemCategoryStore.LookupEntry(requiredTotemCategoryId);
    if (!reqEntry)
        return false;

    if (itemEntry->categoryType != reqEntry->categoryType)
        return false;

    return (itemEntry->categoryMask & reqEntry->categoryMask) ==
           reqEntry->categoryMask;
}

bool Zone2MapCoordinates(float& x, float& y, uint32 zone)
{
    WorldMapAreaEntry const* maEntry = sWorldMapAreaStore.LookupEntry(zone);

    // if not listed then map coordinates (instance)
    if (!maEntry || maEntry->x2 == maEntry->x1 || maEntry->y2 == maEntry->y1)
        return false;

    std::swap(x, y); // at client map coords swapped
    x = x * ((maEntry->x2 - maEntry->x1) / 100) + maEntry->x1;
    y = y * ((maEntry->y2 - maEntry->y1) / 100) +
        maEntry->y1; // client y coord from top to down

    return true;
}

bool Map2ZoneCoordinates(float& x, float& y, uint32 zone)
{
    WorldMapAreaEntry const* maEntry = sWorldMapAreaStore.LookupEntry(zone);

    // if not listed then map coordinates (instance)
    if (!maEntry || maEntry->x2 == maEntry->x1 || maEntry->y2 == maEntry->y1)
        return false;

    x = (x - maEntry->x1) / ((maEntry->x2 - maEntry->x1) / 100);
    y = (y - maEntry->y1) /
        ((maEntry->y2 - maEntry->y1) / 100); // client y coord from top to down
    std::swap(x, y);                         // client have map coords swapped

    return true;
}

uint32 GetTalentInspectBitPosInTab(uint32 talentId)
{
    TalentInspectMap::const_iterator itr = sTalentPosInInspect.find(talentId);
    if (itr == sTalentPosInInspect.end())
        return 0;

    return itr->second;
}

uint32 GetTalentTabInspectBitSize(uint32 talentTabId)
{
    TalentInspectMap::const_iterator itr =
        sTalentTabSizeInInspect.find(talentTabId);
    if (itr == sTalentTabSizeInInspect.end())
        return 0;

    return itr->second;
}

uint32 const* GetTalentTabPages(uint32 cls)
{
    return sTalentTabPages[cls];
}

bool IsPointInAreaTriggerZone(AreaTriggerEntry const* atEntry, uint32 mapid,
    float x, float y, float z, float delta)
{
    if (mapid != atEntry->mapid)
        return false;

    if (atEntry->radius > 0)
    {
        // if we have radius check it
        float dist2 = (x - atEntry->x) * (x - atEntry->x) +
                      (y - atEntry->y) * (y - atEntry->y) +
                      (z - atEntry->z) * (z - atEntry->z);
        if (dist2 > (atEntry->radius + delta) * (atEntry->radius + delta))
            return false;
    }
    else
    {
        // we have only extent

        // rotate the players position instead of rotating the whole cube, that
        // way we can make a simplified
        // is-in-cube check and we have to calculate only one point instead of 4

        // 2PI = 360, keep in mind that ingame orientation is counter-clockwise
        double rotation = 2 * M_PI - atEntry->box_orientation;
        double sinVal = sin(rotation);
        double cosVal = cos(rotation);

        float playerBoxDistX = x - atEntry->x;
        float playerBoxDistY = y - atEntry->y;

        float rotPlayerX = float(
            atEntry->x + playerBoxDistX * cosVal - playerBoxDistY * sinVal);
        float rotPlayerY = float(
            atEntry->y + playerBoxDistY * cosVal + playerBoxDistX * sinVal);

        // box edges are parallel to coordiante axis, so we can treat every
        // dimension independently :D
        float dz = z - atEntry->z;
        float dx = rotPlayerX - atEntry->x;
        float dy = rotPlayerY - atEntry->y;
        if ((fabs(dx) > atEntry->box_x / 2 + delta) ||
            (fabs(dy) > atEntry->box_y / 2 + delta) ||
            (fabs(dz) > atEntry->box_z / 2 + delta))
        {
            return false;
        }
    }

    return true;
}

int WmoLiquidCacheArea(uint32 map, uint32 root, uint32 adt, uint32 group)
{
    // Figure out area of group spawn
    auto entry = GetWMOAreaTableEntryByTripple(root, adt, group);
    if (entry)
    {
        if (entry->areaId == 0)
            return TerrainManager::GetAreaIdByAreaFlag(
                GetAreaFlagByMapId(map), map);
        return entry->areaId;
    }
    return -1;
}

uint32 GetCreatureModelRace(uint32 model_id)
{
    CreatureDisplayInfoEntry const* displayEntry =
        sCreatureDisplayInfoStore.LookupEntry(model_id);
    if (!displayEntry)
        return 0;
    CreatureDisplayInfoExtraEntry const* extraEntry =
        sCreatureDisplayInfoExtraStore.LookupEntry(
            displayEntry->ExtendedDisplayInfoID);
    return extraEntry ? extraEntry->Race : 0;
}

// script support functions
MANGOS_DLL_SPEC DBCStorage<SoundEntriesEntry> const* GetSoundEntriesStore()
{
    return &sSoundEntriesStore;
}
MANGOS_DLL_SPEC DBCStorage<SpellEntry> const* GetSpellStore()
{
    return &sSpellStore;
}
MANGOS_DLL_SPEC DBCStorage<SpellRangeEntry> const* GetSpellRangeStore()
{
    return &sSpellRangeStore;
}
MANGOS_DLL_SPEC DBCStorage<FactionEntry> const* GetFactionStore()
{
    return &sFactionStore;
}
MANGOS_DLL_SPEC DBCStorage<ItemEntry> const* GetItemDisplayStore()
{
    return &sItemStore;
}
MANGOS_DLL_SPEC DBCStorage<CreatureDisplayInfoEntry> const*
GetCreatureDisplayStore()
{
    return &sCreatureDisplayInfoStore;
}
MANGOS_DLL_SPEC DBCStorage<EmotesEntry> const* GetEmotesStore()
{
    return &sEmotesStore;
}
MANGOS_DLL_SPEC DBCStorage<EmotesTextEntry> const* GetEmotesTextStore()
{
    return &sEmotesTextStore;
}

void LoadSpellStoreFromDB(DBCStorage<SpellEntry>& sSpellStore)
{
    QueryResult* rs = WorldDatabase.Query("SELECT MAX(Id) FROM spell_dbc");
    if (!rs)
        throw std::runtime_error("SELECT MAX(Id) FROM spell_dbc returned NULL");
    uint32 max_id = rs->Fetch()->GetUInt32();
    delete rs;

    auto null_char_ptr = new char[1];
    null_char_ptr[0] = '\0';

    auto entries = new SpellEntry* [max_id + 1];

    for (uint32 i = 0; i <= max_id; ++i)
        entries[i] = nullptr;

    QueryResult* result = WorldDatabase.Query("SELECT * FROM spell_dbc");
    if (!result)
        throw std::runtime_error("SELECT * FROM spell_dbc returned NULL");

    uint32 count = 0;
    do
    {
        ++count;

        Field* fields = result->Fetch();
        auto entry = new SpellEntry;

        entry->Id = fields[0].GetUInt32();
        entry->Category = fields[1].GetUInt32();
        entry->SpellSpecific = fields[2].GetUInt32();
        entry->Dispel = fields[3].GetUInt32();
        entry->Mechanic = fields[4].GetUInt32();
        entry->Attributes = fields[5].GetUInt32();
        entry->AttributesEx = fields[6].GetUInt32();
        entry->AttributesEx2 = fields[7].GetUInt32();
        entry->AttributesEx3 = fields[8].GetUInt32();
        entry->AttributesEx4 = fields[9].GetUInt32();
        entry->AttributesEx5 = fields[10].GetUInt32();
        entry->AttributesEx6 = fields[11].GetUInt32();
        entry->Stances = fields[12].GetUInt32();
        entry->StancesNot = fields[13].GetUInt32();
        entry->Targets = fields[14].GetUInt32();
        entry->TargetCreatureType = fields[15].GetUInt32();
        entry->RequiresSpellFocus = fields[16].GetUInt32();
        entry->FacingCasterFlags = fields[17].GetUInt32();
        entry->CasterAuraState = fields[18].GetUInt32();
        entry->TargetAuraState = fields[19].GetUInt32();
        entry->CasterAuraStateNot = fields[20].GetUInt32();
        entry->TargetAuraStateNot = fields[21].GetUInt32();
        entry->CastingTimeIndex = fields[22].GetUInt32();
        entry->RecoveryTime = fields[23].GetUInt32();
        entry->CategoryRecoveryTime = fields[24].GetUInt32();
        entry->InterruptFlags = fields[25].GetUInt32();
        entry->AuraInterruptFlags = fields[26].GetUInt32();
        entry->ChannelInterruptFlags = fields[27].GetUInt32();
        entry->procFlags = fields[28].GetUInt32();
        entry->procChance = fields[29].GetUInt32();
        entry->procCharges = fields[30].GetUInt32();
        entry->maxLevel = fields[31].GetUInt32();
        entry->baseLevel = fields[32].GetUInt32();
        entry->spellLevel = fields[33].GetUInt32();
        entry->DurationIndex = fields[34].GetUInt32();
        entry->powerType = fields[35].GetUInt32();
        entry->manaCost = fields[36].GetUInt32();
        entry->manaCostPerlevel = fields[37].GetUInt32();
        entry->manaPerSecond = fields[38].GetUInt32();
        entry->manaPerSecondPerLevel = fields[39].GetUInt32();
        entry->rangeIndex = fields[40].GetUInt32();
        entry->speed = fields[41].GetFloat();
        entry->StackAmount = fields[42].GetUInt32();
        entry->Totem[0] = fields[43].GetUInt32();
        entry->Totem[1] = fields[44].GetUInt32();
        entry->Reagent[0] = fields[45].GetInt32();
        entry->Reagent[1] = fields[46].GetInt32();
        entry->Reagent[2] = fields[47].GetInt32();
        entry->Reagent[3] = fields[48].GetInt32();
        entry->Reagent[4] = fields[49].GetInt32();
        entry->Reagent[5] = fields[50].GetInt32();
        entry->Reagent[6] = fields[51].GetInt32();
        entry->Reagent[7] = fields[52].GetInt32();
        entry->ReagentCount[0] = fields[53].GetUInt32();
        entry->ReagentCount[1] = fields[54].GetUInt32();
        entry->ReagentCount[2] = fields[55].GetUInt32();
        entry->ReagentCount[3] = fields[56].GetUInt32();
        entry->ReagentCount[4] = fields[57].GetUInt32();
        entry->ReagentCount[5] = fields[58].GetUInt32();
        entry->ReagentCount[6] = fields[59].GetUInt32();
        entry->ReagentCount[7] = fields[60].GetUInt32();
        entry->EquippedItemClass = fields[61].GetInt32();
        entry->EquippedItemSubClassMask = fields[62].GetInt32();
        entry->EquippedItemInventoryTypeMask = fields[63].GetInt32();
        entry->Effect[0] = fields[64].GetUInt32();
        entry->Effect[1] = fields[65].GetUInt32();
        entry->Effect[2] = fields[66].GetUInt32();
        entry->EffectDieSides[0] = fields[67].GetInt32();
        entry->EffectDieSides[1] = fields[68].GetInt32();
        entry->EffectDieSides[2] = fields[69].GetInt32();
        entry->EffectBaseDice[0] = fields[70].GetUInt32();
        entry->EffectBaseDice[1] = fields[71].GetUInt32();
        entry->EffectBaseDice[2] = fields[72].GetUInt32();
        entry->EffectDicePerLevel[0] = fields[73].GetFloat();
        entry->EffectDicePerLevel[1] = fields[74].GetFloat();
        entry->EffectDicePerLevel[2] = fields[75].GetFloat();
        entry->EffectRealPointsPerLevel[0] = fields[76].GetFloat();
        entry->EffectRealPointsPerLevel[1] = fields[77].GetFloat();
        entry->EffectRealPointsPerLevel[2] = fields[78].GetFloat();
        entry->EffectBasePoints[0] = fields[79].GetInt32();
        entry->EffectBasePoints[1] = fields[80].GetInt32();
        entry->EffectBasePoints[2] = fields[81].GetInt32();
        entry->EffectMechanic[0] = fields[82].GetUInt32();
        entry->EffectMechanic[1] = fields[83].GetUInt32();
        entry->EffectMechanic[2] = fields[84].GetUInt32();
        entry->EffectImplicitTargetA[0] = fields[85].GetUInt32();
        entry->EffectImplicitTargetA[1] = fields[86].GetUInt32();
        entry->EffectImplicitTargetA[2] = fields[87].GetUInt32();
        entry->EffectImplicitTargetB[0] = fields[88].GetUInt32();
        entry->EffectImplicitTargetB[1] = fields[89].GetUInt32();
        entry->EffectImplicitTargetB[2] = fields[90].GetUInt32();
        entry->EffectRadiusIndex[0] = fields[91].GetUInt32();
        entry->EffectRadiusIndex[1] = fields[92].GetUInt32();
        entry->EffectRadiusIndex[2] = fields[93].GetUInt32();
        entry->EffectApplyAuraName[0] = fields[94].GetUInt32();
        entry->EffectApplyAuraName[1] = fields[95].GetUInt32();
        entry->EffectApplyAuraName[2] = fields[96].GetUInt32();
        entry->EffectAmplitude[0] = fields[97].GetUInt32();
        entry->EffectAmplitude[1] = fields[98].GetUInt32();
        entry->EffectAmplitude[2] = fields[99].GetUInt32();
        entry->EffectMultipleValue[0] = fields[100].GetFloat();
        entry->EffectMultipleValue[1] = fields[101].GetFloat();
        entry->EffectMultipleValue[2] = fields[102].GetFloat();
        entry->EffectChainTarget[0] = fields[103].GetUInt32();
        entry->EffectChainTarget[1] = fields[104].GetUInt32();
        entry->EffectChainTarget[2] = fields[105].GetUInt32();
        entry->EffectItemType[0] = fields[106].GetUInt32();
        entry->EffectItemType[1] = fields[107].GetUInt32();
        entry->EffectItemType[2] = fields[108].GetUInt32();
        entry->EffectMiscValue[0] = fields[109].GetInt32();
        entry->EffectMiscValue[1] = fields[110].GetInt32();
        entry->EffectMiscValue[2] = fields[111].GetInt32();
        entry->EffectMiscValueB[0] = fields[112].GetInt32();
        entry->EffectMiscValueB[1] = fields[113].GetInt32();
        entry->EffectMiscValueB[2] = fields[114].GetInt32();
        entry->EffectTriggerSpell[0] = fields[115].GetUInt32();
        entry->EffectTriggerSpell[1] = fields[116].GetUInt32();
        entry->EffectTriggerSpell[2] = fields[117].GetUInt32();
        entry->EffectPointsPerComboPoint[0] = fields[118].GetFloat();
        entry->EffectPointsPerComboPoint[1] = fields[119].GetFloat();
        entry->EffectPointsPerComboPoint[2] = fields[120].GetFloat();
        entry->SpellVisual = fields[121].GetUInt32();
        entry->SpellIconID = fields[122].GetUInt32();
        entry->activeIconID = fields[123].GetUInt32();
        // These strings created with new will not get cleaned up properly, and
        // will
        // leak. But since spells are only deleted at program termination, who
        // cares?
        uint32 arr_len = strlen(fields[124].GetString()) + 1;
        auto spell_name = new char[arr_len];
        memset(spell_name, 0, arr_len);
        strcpy(spell_name, fields[124].GetString());
        entry->SpellName[0] = spell_name;
        // FIXME: Fill in these if we want to properly support ".lookup spell"
        // for non-english clients too
        entry->SpellName[1] = null_char_ptr;
        entry->SpellName[2] = null_char_ptr;
        entry->SpellName[3] = null_char_ptr;
        entry->SpellName[4] = null_char_ptr;
        entry->SpellName[5] = null_char_ptr;
        entry->SpellName[6] = null_char_ptr;
        entry->SpellName[7] = null_char_ptr;
        entry->SpellName[8] = null_char_ptr;
        entry->SpellName[9] = null_char_ptr;
        entry->SpellName[10] = null_char_ptr;
        entry->SpellName[11] = null_char_ptr;
        entry->SpellName[12] = null_char_ptr;
        entry->SpellName[13] = null_char_ptr;
        entry->SpellName[14] = null_char_ptr;
        entry->SpellName[15] = null_char_ptr;
        arr_len = strlen(fields[140].GetString()) + 1;
        auto spell_rank = new char[arr_len];
        memset(spell_rank, 0, arr_len);
        strcpy(spell_rank, fields[140].GetString());
        entry->Rank[0] = spell_rank;
        // FIXME: Fill in these if we want to properly support ".lookup spell"
        // for non-english clients too
        entry->Rank[1] = null_char_ptr;
        entry->Rank[2] = null_char_ptr;
        entry->Rank[3] = null_char_ptr;
        entry->Rank[4] = null_char_ptr;
        entry->Rank[5] = null_char_ptr;
        entry->Rank[6] = null_char_ptr;
        entry->Rank[7] = null_char_ptr;
        entry->Rank[8] = null_char_ptr;
        entry->Rank[9] = null_char_ptr;
        entry->Rank[10] = null_char_ptr;
        entry->Rank[11] = null_char_ptr;
        entry->Rank[12] = null_char_ptr;
        entry->Rank[13] = null_char_ptr;
        entry->Rank[14] = null_char_ptr;
        entry->Rank[15] = null_char_ptr;
        entry->ManaCostPercentage = fields[156].GetUInt32();
        entry->StartRecoveryCategory = fields[157].GetUInt32();
        entry->StartRecoveryTime = fields[158].GetUInt32();
        entry->MaxTargetLevel = fields[159].GetUInt32();
        entry->SpellFamilyName = fields[160].GetUInt32();
        entry->SpellFamilyFlags = (ClassFamilyMask)fields[161].GetUInt64();
        entry->MaxAffectedTargets = fields[162].GetUInt32();
        entry->DmgClass = fields[163].GetUInt32();
        entry->PreventionType = fields[164].GetUInt32();
        entry->DmgMultiplier[0] = fields[165].GetFloat();
        entry->DmgMultiplier[1] = fields[166].GetFloat();
        entry->DmgMultiplier[2] = fields[167].GetFloat();
        entry->TotemCategory[0] = fields[168].GetUInt32();
        entry->TotemCategory[1] = fields[169].GetUInt32();
        entry->AreaId = fields[170].GetUInt32();
        entry->SchoolMask = fields[171].GetUInt32();
        entry->aoe_cap = fields[172].GetUInt32();
        entry->AttributesCustom = fields[173].GetUInt32();
        entry->AttributesCustom1 = fields[174].GetUInt32();
        entry->bounce_radius = fields[175].GetUInt32();

        entries[entry->Id] = entry;
    } while (result->NextRow());

    delete result;

    sSpellStore.ReplaceIndexTable(entries, max_id + 1);
}
