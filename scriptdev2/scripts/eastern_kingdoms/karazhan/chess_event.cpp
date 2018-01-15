/* Copyright (C) 2012 Corecraft (WorldofCoreCraft.com) */

/* ScriptData
SDName: chess_event
SD%Complete: 100
SDComment:
SDCategory: Karazhan
EndScriptData */

#include "karazhan.h"
#include "precompiled.h"

enum
{
    SAY_MEDIVH_START_GAME = 10338,
    SAY_MEDIVH_PAWN_TAKEN_1 = 10339,
    SAY_MEDIVH_PAWN_TAKEN_2 = 10340,
    SAY_MEDIVH_PAWN_TAKEN_3 = 10341,
    SAY_MEDIVH_PAWN_LOST_1 = 10342,
    SAY_MEDIVH_PAWN_LOST_2 = 10343,
    SAY_MEDIVH_PAWN_LOST_3 = 10344,
    SAY_MEDIVH_ROOK_TAKEN = 10345,
    SAY_MEDIVH_ROOK_LOST = 10346,
    SAY_MEDIVH_BISHOP_TAKEN = 10347,
    SAY_MEDIVH_BISHOP_LOST = 10348,
    SAY_MEDIVH_KNIGHT_TAKEN = 10349,
    SAY_MEDIVH_KNIGHT_LOST = 10350,
    SAY_MEDIVH_QUEEN_TAKEN = 10351,
    SAY_MEDIVH_QUEEN_LOST = 10352,
    SAY_MEDIVH_CHECK = 10353,
    SAY_PLAYER_CHECK = 10354,
    SAY_MEDIVH_CHECKMATE = 10355,
    SAY_PLAYER_CHECKMATE = 10356,
    SAY_MEDIVH_CHEAT_1 = 10357,
    SAY_MEDIVH_CHEAT_2 = 10358,
    SAY_MEDIVH_CHEAT_3 = 10359,
    EMOTE_WIN = -1530100,

    SPELL_CONTROL_PIECE = 30019,
    SPELL_RECENTLY_IN_GAME = 30529,
    NPC_EMPTY_SPACE = 80001,

    NPC_HUMAN_FOOTMAN = 17211,
    NPC_CONJURED_WATER_ELEMENTAL = 21160,
    NPC_HUMAN_CHARGER = 21664,
    NPC_HUMAN_CLERIC = 21682,
    NPC_HUMAN_CONJURER = 21683,
    NPC_KING_LLANE = 21684,

    NPC_ORC_GRUNT = 17469,
    NPC_SUMMONED_DAEMON = 21726,
    NPC_ORC_WOLF = 21748,
    NPC_ORC_NECROLYTE = 21747,
    NPC_WARCHIEF_BLACKHAND = 21752,
    NPC_ORC_WARLOCK = 21750,

    SPELL_NORMAL_MOVE = 37153,
    SPELL_HORSE_MOVE = 37144,
    SPELL_QUEEN_MOVE = 37148,
    SPELL_CHANGE_FACING = 30284,

    SPELL_RED_MARKER = 32745,
    SPELL_MOVE_MARKER = 32261,

    SPELL_CHEAT_FURY = 39383,
    SPELL_CHEAT_HAND = 39339,
    NPC_FURY_OF_MEDIVH = 22521,

    SPELL_VICTORY_SHAKE = 39395,
    SPELL_GAME_OVER_VISUAL = 39401,
    SPELL_RESURRECTION_VISUAL = 32343,
    SPELL_CAMERA_SHAKE = 39983,

    SPELL_MELEE_ATTACK_DAMAGE = 32247,
};

#define IN_FRONT_F 2 * M_PI_F / 3
#define IN_FRONT_90_F M_PI_F / 2
#define IN_FRONT_15_F M_PI_F / 12

struct ChessPiecePos
{
    float X, Y, Z, O;
};

/* PIECES SPAWN POSITIONS */
const ChessPiecePos HumanFootmanPos[] = {
    // 17211
    {-11082.9f, -1895.75f, 220.667f, 0.759299f},
    {-11079.4f, -1900.17f, 220.667f, 0.649344f},
    {-11103.7f, -1869.63f, 220.667f, 0.661124f},
    {-11100.2f, -1873.96f, 220.668f, 0.597337f},
    {-11096.8f, -1878.21f, 220.668f, 0.593409f},
    {-11089.7f, -1887.03f, 220.667f, 0.617927f},
    {-11086.2f, -1891.53f, 220.667f, 0.617928f},
    {-11093.4f, -1882.59f, 220.668f, 0.671956f}};

const ChessPiecePos ConjuredWaterElementalPos[] = {
    // 21160
    {-11108.1f, -1872.91f, 220.667f, 0.679031f},
    {-11083.7f, -1903.94f, 220.667f, 0.671177f},
};

const ChessPiecePos HumanChargerPos[] = {
    // 21664
    {-11104.4f, -1877.70f, 220.667f, 0.592640f},
    {-11087.2f, -1899.56f, 220.667f, 0.624056f},
};

const ChessPiecePos HumanClericPos[] = {
    // 21682
    {-11101.0f, -1881.83f, 220.667f, 0.598288f},
    {-11090.5f, -1895.02f, 220.667f, 0.665045f},
};

const ChessPiecePos HumanConjurer = {
    -11097.6f, -1886.43f, 220.667f, 0.723955f}; // 21683

const ChessPiecePos KingLlane = {
    -11094.1f, -1890.57f, 220.667f, 0.62578f}; // 21684

const ChessPiecePos OrcGrunt[] = {
    // 17469
    {-11082.1f, -1852.43f, 220.667f, 3.80908f},
    {-11071.5f, -1865.54f, 220.667f, 3.80908f},
    {-11061.0f, -1878.56f, 220.667f, 3.80908f},
    {-11064.3f, -1874.30f, 220.667f, 3.80908f},
    {-11067.8f, -1869.89f, 220.667f, 3.80908f},
    {-11057.4f, -1883.03f, 220.667f, 3.81300f},
    {-11078.5f, -1856.93f, 220.667f, 3.80908f},
    {-11075.0f, -1861.13f, 220.667f, 3.80908f},
};

const ChessPiecePos SummonedDaemon[] = {
    // 21726
    {-11053.4f, -1879.64f, 220.667f, 3.89304f},
    {-11077.5f, -1848.75f, 220.667f, 3.80665f},
};

const ChessPiecePos OrcWolf[] = {
    // 21748
    {-11074.0f, -1853.28f, 220.667f, 3.84199f},
    {-11056.7f, -1875.07f, 220.667f, 3.7698f},
};

const ChessPiecePos OrcNecrolyte[] = {
    // 21747
    {-11070.8f, -1857.7f, 220.667f, 3.81692f},
    {-11060.3f, -1870.81f, 220.667f, 3.76195f},
};

const ChessPiecePos WarchiefBlackhand = {
    -11063.6f, -1866.47f, 220.667f, 3.84835f}; // 21752

const ChessPiecePos OrcWarlock = {
    -11067.0f, -1861.86f, 220.667f, 3.72268f}; // 21750

const ChessPiecePos EmptySpace[] = {
    // 80001
    {-11084.5f, -1876.03f, 220.668f, 0.667250f},
    {-11073.2f, -1881.22f, 220.668f, 0.659391f},
    {-11076.5f, -1876.83f, 220.668f, 0.659389f},
    {-11085.5f, -1883.72f, 220.668f, 0.655462f},
    {-11082.1f, -1888.08f, 220.668f, 0.647610f},
    {-11065.5f, -1882.11f, 220.668f, 0.679023f},
    {-11074.0f, -1888.96f, 220.668f, 0.679023f},
    {-11075.0f, -1896.96f, 220.668f, 0.668592f},
    {-11068.9f, -1877.85f, 220.668f, 0.659391f},
    {-11081.0f, -1880.32f, 220.668f, 0.651535f},
    {-11072.4f, -1873.61f, 220.668f, 0.659389f},
    {-11075.7f, -1869.14f, 220.668f, 0.667250f},
    {-11088.8f, -1879.40f, 220.668f, 0.734009f},
    {-11080.0f, -1872.46f, 220.668f, 0.667250f},
    {-11077.6f, -1884.59f, 220.668f, 0.659391f},
    {-11069.9f, -1885.66f, 220.668f, 0.679023f},
    {-11078.5f, -1892.50f, 220.668f, 0.671169f},
    {-11070.6f, -1893.46f, 220.668f, 0.668592f},
    {-11066.4f, -1890.17f, 220.668f, 0.668592f},
    {-11062.0f, -1886.70f, 220.668f, 0.668592f},
    {-11092.3f, -1874.87f, 220.668f, 0.672519f},
    {-11088.0f, -1871.48f, 220.668f, 0.668592f},
    {-11083.6f, -1868.01f, 220.668f, 0.668592f},
    {-11079.4f, -1864.70f, 220.668f, 0.668592f},
    {-11095.9f, -1870.53f, 220.668f, 0.688226f},
    {-11091.6f, -1867.30f, 220.668f, 0.649816f},
    {-11087.2f, -1863.68f, 220.668f, 0.685159f},
    {-11083.0f, -1860.21f, 220.668f, 0.685159f},
    {-11099.4f, -1866.02f, 220.668f, 0.685447f},
    {-11095.3f, -1862.60f, 220.668f, 0.689374f},
    {-11090.9f, -1859.40f, 220.668f, 0.638323f},
    {-11086.5f, -1855.99f, 220.668f, 0.661885f}};

ChessPiecePos StandDeadHorde = {-11079.0f, -1842.6f, 221.1f, 5.4f};
ChessPiecePos StandDeadAlliance = {-11082.4f, -1909.8f, 221.1f, 2.27f};

/* GENERAL PIECE: CLASSES & FUNCTIONS */
struct MANGOS_DLL_DECL mob_chess_pieceAI : public ScriptedAI
{
    mob_chess_pieceAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_uiStartTimer = 2000;
        m_teamPlaying = TEAM_NONE;
        m_pInstance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                          (instance_karazhan*)pCreature->GetInstanceData() :
                          NULL;
        if (!m_pInstance)
            return;
        uint64 controller = m_pInstance->GetData64(m_creature->GetGUIDLow());
        if (controller != 0)
        {
            m_bIsControlled = true;
            m_uiControlledCheckTimer = 0;
            m_controlledByGuid = ObjectGuid(controller);
        }
        else
        {
            m_bIsControlled = false;
            m_uiControlledCheckTimer = 0;
            m_controlledByGuid.Set(0);
        }

        EncounterState es = (EncounterState)m_pInstance->GetData(TYPE_CHESS);
        if (es == NOT_STARTED || es == FAIL)
            m_bIsGameStarted = false;
        else if (es != DONE)
            AlertPieceOfGameStart();

        m_uiMeleeAttackTimer = urand(15000, 30000);

        m_bIsJustADoll = false;
        m_uiStartTimer = 0;

        Reset();
    }

    bool m_bIsJustADoll;

    instance_karazhan* m_pInstance;
    bool m_bIsControlled;
    ObjectGuid m_controlledByGuid;
    uint32 m_uiControlledCheckTimer;

    // Timers will be 0 when ready
    uint32 m_uiMoveTimer;
    uint32 m_uiFacingCDTimer;
    uint32 m_uiMeleeAttackTimer;
    uint32 m_uiSpecialAttackTimer;

    uint32 m_uiStartTimer;

    ObjectGuid m_EnemyKing; // Set by Medivh

    Team m_teamPlaying;
    bool m_bIsGameStarted;

    void BeforeDeath(Unit*) override
    {
        if (auto charmer = m_creature->GetCharmer())
            charmer->remove_auras(SPELL_CONTROL_PIECE);

        // Summon spawn marker
        float x, y, z, o;
        if (auto idle = dynamic_cast<movement::IdleMovementGenerator*>(
                m_creature->movement_gens.get(movement::gen::idle)))
        {
            x = idle->x_;
            y = idle->y_;
            z = idle->z_;
            o = idle->o_;
        }
        else
        {
            m_creature->GetPosition(x, y, z);
            o = m_creature->GetO();
        }

        m_creature->SummonCreature(
            NPC_EMPTY_SPACE, x, y, z, o, TEMPSUMMON_MANUAL_DESPAWN, 0);
    }

    virtual void DamageTaken(Unit* pDealer, uint32& uiDamage) override
    {
        if (!m_pInstance)
            return;
        if (m_pInstance->GetData(TYPE_CHESS) != IN_PROGRESS)
            uiDamage = 0;
        m_creature->SetInCombatWith(pDealer); // So AoE functions correctly
    }

    virtual void AlertPieceOfGameStart()
    {
        if (!m_pInstance)
            return;
        m_teamPlaying = (Team)m_pInstance->GetData(DATA_CHESS_PLAYING_TEAM);
        m_bIsGameStarted = true;
    }

    virtual void JustDied(Unit* /*pKiller*/) override
    {
        if (!m_pInstance)
            return;

        // Summon smaller copy of self next to board
        Team pieceTeam = TEAM_NONE;
        uint32 e = m_creature->GetEntry();
        if (e == 21752 || e == 21750 || e == 21747 || e == 21748 ||
            e == 21726 || e == 17469) // Horde pieces
            pieceTeam = HORDE;
        else if (e == 21684 || e == 21683 || e == 21682 || e == 21664 ||
                 e == 21160 || e == 17211) // Alliance pieces
            pieceTeam = ALLIANCE;

        if (pieceTeam == ALLIANCE)
        {
            uint32 deadPieces =
                m_pInstance->GetData(DATA_CHESS_DEAD_ALLIANCE_PIECES);
            float x = StandDeadAlliance.X;
            float y = StandDeadAlliance.Y;
            float z = StandDeadAlliance.Z;
            float o = StandDeadAlliance.O;
            if (deadPieces > 0)
            {
                float x_delta = 1.3f * uint32(deadPieces / 2);
                float y_delta = 1.0f * uint32(deadPieces / 2);

                if (deadPieces % 2 != 0) // Are we in the second row?
                {
                    x_delta += 1.0f;
                    y_delta += -1.1f;
                }

                x += x_delta;
                y += y_delta;
            }

            Creature* pMiniature =
                m_creature->SummonCreature(m_creature->GetEntry(), x, y, z, o,
                    TEMPSUMMON_MANUAL_DESPAWN, 0);
            if (pMiniature)
            {
                pMiniature->SetObjectScale(0.4f);
                pMiniature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                m_pInstance->m_cleanupMiniatures.push_back(
                    pMiniature->GetObjectGuid());
                if (mob_chess_pieceAI* pMinAI =
                        dynamic_cast<mob_chess_pieceAI*>(pMiniature->AI()))
                    pMinAI->m_bIsJustADoll = true;
            }
            m_pInstance->SetData(
                DATA_CHESS_DEAD_ALLIANCE_PIECES, DEAD_PIECE_INCREASE);
        }
        else if (pieceTeam == HORDE)
        {
            uint32 deadPieces =
                m_pInstance->GetData(DATA_CHESS_DEAD_HORDE_PIECES);
            float x = StandDeadHorde.X;
            float y = StandDeadHorde.Y;
            float z = StandDeadHorde.Z;
            float o = StandDeadHorde.O;
            if (deadPieces > 0)
            {
                float x_delta = -1.3f * uint32(deadPieces / 2);
                float y_delta = -1.0f * uint32(deadPieces / 2);

                if (deadPieces % 2 != 0) // Are we in the second row?
                {
                    x_delta += -1.0f;
                    y_delta += 1.1f;
                }

                x += x_delta;
                y += y_delta;
            }

            Creature* pMiniature =
                m_creature->SummonCreature(m_creature->GetEntry(), x, y, z, o,
                    TEMPSUMMON_MANUAL_DESPAWN, 0);
            if (pMiniature)
            {
                pMiniature->SetObjectScale(0.4f);
                pMiniature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                m_pInstance->m_cleanupMiniatures.push_back(
                    pMiniature->GetObjectGuid());
                if (mob_chess_pieceAI* pMinAI =
                        dynamic_cast<mob_chess_pieceAI*>(pMiniature->AI()))
                    pMinAI->m_bIsJustADoll = true;
            }

            m_pInstance->SetData(
                DATA_CHESS_DEAD_HORDE_PIECES, DEAD_PIECE_INCREASE);
        }

        // Delayed despawn
        m_creature->ForcedDespawn(1000);

        // Do Talk Stuff With Medivh

        uint32 soundid = 0;
        switch (m_creature->GetEntry())
        {
        // PAWN
        case 17211:
        case 17469:
            if (m_teamPlaying == pieceTeam)
            {
                switch (urand(0, 2))
                {
                case 0:
                    soundid = SAY_MEDIVH_PAWN_TAKEN_1;
                    break;
                case 1:
                    soundid = SAY_MEDIVH_PAWN_TAKEN_2;
                    break;
                case 2:
                    soundid = SAY_MEDIVH_PAWN_TAKEN_3;
                    break;
                }
            }
            else
            {
                switch (urand(0, 2))
                {
                case 0:
                    soundid = SAY_MEDIVH_PAWN_LOST_1;
                    break;
                case 1:
                    soundid = SAY_MEDIVH_PAWN_LOST_2;
                    break;
                case 2:
                    soundid = SAY_MEDIVH_PAWN_LOST_3;
                    break;
                }
            }
            break;

        // ROOK
        case 21160:
        case 21726:
            if (m_teamPlaying == pieceTeam)
                soundid = SAY_MEDIVH_ROOK_TAKEN;
            else
                soundid = SAY_MEDIVH_ROOK_LOST;
            break;

        // BISHOP
        case 21682:
        case 21747:
            if (m_teamPlaying == pieceTeam)
                soundid = SAY_MEDIVH_BISHOP_TAKEN;
            else
                soundid = SAY_MEDIVH_BISHOP_LOST;
            break;

        // KNIGHT
        case 21664:
        case 21748:
            if (m_teamPlaying == pieceTeam)
                soundid = SAY_MEDIVH_KNIGHT_TAKEN;
            else
                soundid = SAY_MEDIVH_KNIGHT_LOST;
            break;

        // QUEEN
        case 21683:
        case 21750:
            if (m_teamPlaying == pieceTeam)
                soundid = SAY_MEDIVH_QUEEN_TAKEN;
            else
                soundid = SAY_MEDIVH_QUEEN_LOST;
            break;
        }

        if (soundid != 0)
        {
            m_creature->GetMap()->PlayDirectSoundToMap(
                soundid, m_creature->GetZoneId());
        }
    }

    void Reset() override {}

    void DamageDeal(Unit* pVictim, uint32& uiDamage) override
    {
        uint32 e = pVictim->GetEntry();
        if (!(e == 21752 || e == 21750 || e == 21747 || e == 21748 ||
                e == 21726 || e == 17469 || e == 21684 || e == 21683 ||
                e == 21682 || e == 21664 || e == 21160 || e == 17211))
            uiDamage = 0;
    }

    // Disable combat:
    void MoveInLineOfSight(Unit* /*pWho*/) override {}
    void Aggro(Unit* /*pWho*/) override {}
    void AttackStart(Unit* /*pWho*/) override {}
    void EnterCombat(Unit* /*pEnemy*/) override {}
    void AttackedBy(Unit* /*pAttacker*/) override {}

    void EnterEvadeMode(bool = false) override {}

    bool IsAvailableForControlBy(Player* pPlayer)
    {
        return (!m_creature->has_aura(SPELL_CONTROL_PIECE) &&
                !pPlayer->has_aura(SPELL_RECENTLY_IN_GAME));
    }

    /* MOVE LOGIC */
    Unit* GetMoveInFrontOfMe(
        float distance = 9.0f, bool try_for_always_move = false)
    {
        auto unitList = GetCreatureListWithEntryInGrid(
            m_creature, NPC_EMPTY_SPACE, distance);

        std::list<Unit*> nintydegrees;
        std::list<Unit*> desperatelist;
        if (unitList.size() > 0)
        {
            for (auto& elem : unitList)
            {
                // Move right in front of us?
                if (m_creature->isInFront((elem), distance, IN_FRONT_15_F))
                    return (elem);
                // Strafe move available?
                else if (m_creature->isInFront((elem), distance, IN_FRONT_90_F))
                    nintydegrees.push_back(elem);
                // Going left and right? (only if try_for_always_move)
                else if (try_for_always_move &&
                         m_creature->isInFront((elem), distance, IN_FRONT_F))
                    desperatelist.push_back(elem);
            }
        }

        if (nintydegrees.size() > 0)
            return nintydegrees.front();

        if (try_for_always_move && desperatelist.size() > 0)
            return desperatelist.front();

        return NULL;
    }

    Unit* GetSpellReachableTarget(bool friendly_spell = false,
        float spell_dist = 20.0f, float max_hp = 101)
    {
        std::vector<Creature*> unitList;
        if (friendly_spell)
            unitList = GetFriendlyCreatureListInGrid(m_creature, spell_dist);
        else
            unitList = GetUnfriendlyCreatureListInGrid(m_creature, spell_dist);

        Unit* pTarget = NULL;
        if (unitList.size() > 0)
        {
            for (auto& elem : unitList)
            {
                if (IsEnemyPiece((elem)->GetEntry()) == friendly_spell ||
                    !(elem)->isAlive() || (elem)->GetHealthPercent() > max_hp)
                    continue;

                if (m_creature->isInFront(
                        (elem), 8.0f, IN_FRONT_90_F)) // Favour targets in front
                    return elem;
                pTarget = elem;
            }
        }

        return pTarget;
    }

    Unit* GetMeleeReachableTarget()
    {
        auto unitList = GetUnfriendlyCreatureListInGrid(m_creature, 7.0f);

        Unit* pTarget = NULL;
        if (unitList.size() > 0)
        {
            for (auto& elem : unitList)
            {
                if (!IsEnemyPiece((elem)->GetEntry()) || (elem)->isDead())
                    continue;

                if (m_creature->isInFront((elem), 7.0f,
                        IN_FRONT_15_F)) // Favour targets in front of us
                    return (elem);

                pTarget = (elem);
            }
        }

        return pTarget;
    }

    Unit* EnemyPieceInFrontOfMe()
    {
        auto unitList = GetUnfriendlyCreatureListInGrid(m_creature, 7.0f);

        if (unitList.size() > 0)
        {
            for (auto& elem : unitList)
            {
                if (!IsEnemyPiece((elem)->GetEntry()) || (elem)->isDead())
                    continue;

                if (m_creature->isInFront((elem), 7.0f, IN_FRONT_15_F))
                    return (elem);
            }
        }

        return NULL;
    }

    bool IsEnemyPiece(uint32 entry)
    {
        uint32 e = m_creature->GetEntry();
        if (e == 21752 || e == 21750 || e == 21747 || e == 21748 ||
            e == 21726 || e == 17469) // Horde Pieces
        {
            e = entry;
            if (!(e == 21684 || e == 21683 || e == 21682 || e == 21664 ||
                    e == 21160 || e == 17211)) // Alliance Pieces
                return false;
        }
        else if (e == 21684 || e == 21683 || e == 21682 || e == 21664 ||
                 e == 21160 || e == 17211) // Alliance Pieces
        {
            e = entry;
            if (!(e == 21752 || e == 21750 || e == 21747 || e == 21748 ||
                    e == 21726 || e == 17469)) // Horde Pieces
                return false;
        }

        return true;
    }

    virtual void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_pInstance)
            return;
        // In case we're a doll:
        if (m_uiStartTimer)
        {
            if (m_uiStartTimer <= uiDiff)
            {
                m_uiStartTimer = 0;
            }
            else
            {
                m_uiStartTimer -= uiDiff;
                return;
            }
        }

        if (m_bIsJustADoll)
            return;

        if (!m_bIsGameStarted)
            return;

        if (m_creature->GetTargetGuid() != 0)
        {
            m_creature->SetTargetGuid(ObjectGuid());
        }

        if (m_uiMeleeAttackTimer <= uiDiff)
        {
            if (auto unit = EnemyPieceInFrontOfMe())
                m_creature->CastSpell(unit, SPELL_MELEE_ATTACK_DAMAGE, true);
            // See spell 32226 (3 sec amplitude)
            m_uiMeleeAttackTimer = 3000;
        }
        else
            m_uiMeleeAttackTimer -= uiDiff;

        // Update Timers:
        if (m_uiMoveTimer)
        {
            if (m_uiMoveTimer <= uiDiff)
                m_uiMoveTimer = 0;
            else
                m_uiMoveTimer -= uiDiff;
        }
        if (m_uiFacingCDTimer)
        {
            if (m_uiFacingCDTimer <= uiDiff)
                m_uiFacingCDTimer = 0;
            else
                m_uiFacingCDTimer -= uiDiff;
        }

        if (m_uiSpecialAttackTimer)
        {
            if (m_uiSpecialAttackTimer <= uiDiff)
                m_uiSpecialAttackTimer = 0;
            else
                m_uiSpecialAttackTimer -= uiDiff;
        }

        if (!m_bIsControlled && m_creature->has_aura(SPELL_CONTROL_PIECE))
        {
            m_controlledByGuid = m_creature->GetCharmerGuid();
            m_pInstance->SetData64(
                m_creature->GetGUIDLow(), m_controlledByGuid.GetRawValue());
            m_uiControlledCheckTimer = 500;
            m_bIsControlled = true;
        }

        if (m_bIsControlled)
        {
            if (m_uiControlledCheckTimer <= uiDiff)
            {
                m_uiControlledCheckTimer = 500;

                if (Player* pPlayer =
                        m_creature->GetMap()->GetPlayer(m_controlledByGuid))
                {
                    if (!pPlayer->has_aura(SPELL_CONTROL_PIECE))
                    {
                        m_bIsControlled = false;
                        m_controlledByGuid.Set(0);
                        m_pInstance->SetData64(m_creature->GetGUIDLow(), 0);

                        m_creature->remove_auras(SPELL_CONTROL_PIECE);
                        pPlayer->AddAuraThroughNewHolder(
                            SPELL_RECENTLY_IN_GAME, m_creature);

                        if (Creature* pKing =
                                m_creature->GetMap()->GetCreature(m_EnemyKing))
                        {
                            m_creature->SetInCombatWith(pKing);
                            pKing->SetInCombatWith(m_creature);
                        }
                    }
                    else
                    {
                        // Was dismiss used instead?
                        if (!m_creature->has_aura(SPELL_CONTROL_PIECE))
                        {
                            m_bIsControlled = false;
                            m_controlledByGuid.Set(0);
                            m_pInstance->SetData64(m_creature->GetGUIDLow(), 0);

                            pPlayer->remove_auras(SPELL_CONTROL_PIECE);
                            pPlayer->AddAuraThroughNewHolder(
                                SPELL_RECENTLY_IN_GAME, m_creature);

                            if (Creature* pKing =
                                    m_creature->GetMap()->GetCreature(
                                        m_EnemyKing))
                            {
                                m_creature->SetInCombatWith(pKing);
                                pKing->SetInCombatWith(m_creature);
                            }
                        }
                    }
                }
            }
            else
                m_uiControlledCheckTimer -= uiDiff;
        }
    }
};

enum MedivhCheat
{
    CHEAT_HAND = 0,
    CHEAT_FURY,
};

ptrdiff_t medivh_shufflerng(ptrdiff_t i)
{
    return urand(0, i - 1);
}

struct MANGOS_DLL_DECL mob_chess_fury_cheatAI : public ScriptedAI
{
    mob_chess_fury_cheatAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        Reset();
    }
    uint32 m_uiLastDmgTimer;

    // Disable combat:
    void MoveInLineOfSight(Unit* /*pWho*/) override {}
    void Aggro(Unit* /*pWho*/) override {}
    void AttackStart(Unit* /*pWho*/) override {}
    void EnterCombat(Unit* /*pEnemy*/) override {}

    void DamageTaken(Unit* /*pDealer*/, uint32& uiDamage) override
    {
        uiDamage = 0;
    }

    void DamageDeal(Unit* pVictim, uint32& uiDamage) override
    {
        uint32 e = pVictim->GetEntry();
        if (!(e == 21752 || e == 21750 || e == 21747 || e == 21748 ||
                e == 21726 || e == 17469 || e == 21684 || e == 21683 ||
                e == 21682 || e == 21664 || e == 21160 || e == 17211))
        {
            uiDamage = 0;
        }
        else
        {
            if (m_uiLastDmgTimer < 4000)
                uiDamage = 0;
            else
            {
                m_creature->SetInCombatWith(pVictim);
                uiDamage = 10000;
                m_uiLastDmgTimer = 0;
            }
        }
    }

    void Reset() override
    {
        m_uiLastDmgTimer = 0;
        m_creature->SetInCombatWithZone();
    }

    void UpdateAI(const uint32 uiDiff) override { m_uiLastDmgTimer += uiDiff; }
};
CreatureAI* GetAI_mob_chess_fury_cheat(Creature* pCreature)
{
    return new mob_chess_fury_cheatAI(pCreature);
}

/* MEDIVH */
struct MANGOS_DLL_DECL boss_chess_echo_of_medivhAI : public ScriptedAI
{
    boss_chess_echo_of_medivhAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsEventStarted = false;
        m_pInstance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                          (instance_karazhan*)pCreature->GetInstanceData() :
                          NULL;
        SpawnBoard();

        m_uiKingDeathTimer = 0;
        m_uiRespawnBoardTimer = 0;

        m_bIsEventStarted = false;
        m_uiNextCheatTimer = 0;
        m_nextCheat = CHEAT_HAND;

        m_uinextVictoryRain = 0;
        m_uiMaxVictoryTime = 0;
    }

    void Reset() override {}

    instance_karazhan* m_pInstance;

    std::vector<ObjectGuid> m_AlliancePieces;
    std::vector<ObjectGuid> m_HordePieces;
    std::vector<ObjectGuid> m_EmptySpaces;
    std::vector<ObjectGuid> m_SpawnedFurys;

    ObjectGuid m_KingLlane;
    ObjectGuid m_WarchiefBlackhand;

    uint32 m_uiKingDeathTimer;
    uint32 m_uiRespawnBoardTimer;

    bool m_bIsEventStarted;
    uint32 m_uiNextCheatTimer;
    MedivhCheat m_nextCheat;

    uint32 m_uinextVictoryRain;
    uint32 m_uiMaxVictoryTime;

    void SpawnNpc(uint32 entry, ChessPiecePos pos)
    {
        Creature* pCreature = m_creature->SummonCreature(
            entry, pos.X, pos.Y, pos.Z, pos.O, TEMPSUMMON_MANUAL_DESPAWN, 0);
        if (pCreature)
        {
            uint32 e = entry;
            if (e == 21752 || e == 21750 || e == 21747 || e == 21748 ||
                e == 21726 || e == 17469) // Horde Pieces
                m_HordePieces.push_back(pCreature->GetObjectGuid());
            else if (e == 21684 || e == 21683 || e == 21682 || e == 21664 ||
                     e == 21160 || e == 17211) // Alliance Pieces
                m_AlliancePieces.push_back(pCreature->GetObjectGuid());
            else if (e == 80001)
                m_EmptySpaces.push_back(pCreature->GetObjectGuid());

            if (pCreature->GetEntry() == 21684)
                m_KingLlane = pCreature->GetObjectGuid();
            else if (pCreature->GetEntry() == 21752)
                m_WarchiefBlackhand = pCreature->GetObjectGuid();
        }
    }

    void SpawnNpcList(
        uint32 entry, const ChessPiecePos* list, uint32 size_in_bytes)
    {
        uint32 elements = size_in_bytes / sizeof(ChessPiecePos);
        for (uint32 i = 0; i < elements; ++i)
        {
            SpawnNpc(entry, list[i]);
        }
    }

    void SpawnBoard()
    {
        if (!m_pInstance || m_pInstance->GetData(TYPE_CHESS) == DONE)
            return;

        SpawnNpcList(
            NPC_HUMAN_FOOTMAN, HumanFootmanPos, sizeof(HumanFootmanPos));
        SpawnNpcList(NPC_CONJURED_WATER_ELEMENTAL, ConjuredWaterElementalPos,
            sizeof(ConjuredWaterElementalPos));
        SpawnNpcList(
            NPC_HUMAN_CHARGER, HumanChargerPos, sizeof(HumanChargerPos));
        SpawnNpcList(NPC_HUMAN_CLERIC, HumanClericPos, sizeof(HumanClericPos));
        SpawnNpc(NPC_HUMAN_CONJURER, HumanConjurer);
        SpawnNpc(NPC_KING_LLANE, KingLlane);

        SpawnNpcList(NPC_ORC_GRUNT, OrcGrunt, sizeof(OrcGrunt));
        SpawnNpcList(
            NPC_SUMMONED_DAEMON, SummonedDaemon, sizeof(SummonedDaemon));
        SpawnNpcList(NPC_ORC_WOLF, OrcWolf, sizeof(OrcWolf));
        SpawnNpcList(NPC_ORC_NECROLYTE, OrcNecrolyte, sizeof(OrcNecrolyte));
        SpawnNpc(NPC_WARCHIEF_BLACKHAND, WarchiefBlackhand);
        SpawnNpc(NPC_ORC_WARLOCK, OrcWarlock);

        SpawnNpcList(NPC_EMPTY_SPACE, EmptySpace, sizeof(EmptySpace));
    }

    void WipeBoard()
    {
        if (!m_pInstance)
            return;

        for (auto& elem : m_HordePieces)
        {
            Creature* pCreature = m_creature->GetMap()->GetCreature(elem);
            if (pCreature)
            {
                pCreature->ForcedDespawn();
            }
        }

        for (auto& elem : m_AlliancePieces)
        {
            Creature* pCreature = m_creature->GetMap()->GetCreature(elem);
            if (pCreature)
                pCreature->ForcedDespawn();
        }

        for (auto& elem : m_EmptySpaces)
        {
            Creature* pCreature = m_creature->GetMap()->GetCreature(elem);
            if (pCreature)
                pCreature->ForcedDespawn();
        }

        for (auto& elem : m_pInstance->m_EmptySpots)
        {
            Creature* pCreature = m_creature->GetMap()->GetCreature(elem);
            if (pCreature)
                pCreature->ForcedDespawn();
        }

        for (auto& elem : m_SpawnedFurys)
        {
            Creature* pCreature = m_creature->GetMap()->GetCreature(elem);
            if (pCreature)
                pCreature->ForcedDespawn();
        }

        for (auto& elem : m_pInstance->m_cleanupMiniatures)
        {
            Creature* pCreature = m_creature->GetMap()->GetCreature(elem);
            if (pCreature)
                pCreature->ForcedDespawn();
        }

        m_SpawnedFurys.clear();
        m_HordePieces.clear();
        m_AlliancePieces.clear();
        m_EmptySpaces.clear();
        m_pInstance->m_EmptySpots.clear();
        m_pInstance->m_cleanupMiniatures.clear();
    }

    void StartEvent()
    {
        if (!m_pInstance)
            return;

        m_pInstance->SetData(DATA_CHESS_DEAD_ALLIANCE_PIECES, 0);
        m_pInstance->SetData(DATA_CHESS_DEAD_HORDE_PIECES, 0);

        for (auto& elem : m_AlliancePieces)
        {
            Creature* pCreature = m_creature->GetMap()->GetCreature(elem);
            if (pCreature)
            {
                if (mob_chess_pieceAI* pAI =
                        dynamic_cast<mob_chess_pieceAI*>(pCreature->AI()))
                {
                    pAI->AlertPieceOfGameStart();
                    pCreature->SetInCombatWithZone();
                    pAI->m_EnemyKing = m_WarchiefBlackhand;
                    pCreature->SummonCreature(80004, pCreature->GetX(),
                        pCreature->GetY(), pCreature->GetZ(), pCreature->GetO(),
                        TEMPSUMMON_TIMED_DESPAWN, 4000);
                    pCreature->HandleEmoteCommand(EMOTE_ONESHOT_SALUTE);
                }
            }
        }

        for (auto& elem : m_HordePieces)
        {
            Creature* pCreature = m_creature->GetMap()->GetCreature(elem);
            if (pCreature)
            {
                if (mob_chess_pieceAI* pAI =
                        dynamic_cast<mob_chess_pieceAI*>(pCreature->AI()))
                {
                    pAI->AlertPieceOfGameStart();
                    pCreature->SetInCombatWithZone();
                    pAI->m_EnemyKing = m_KingLlane;
                    pCreature->SummonCreature(80004, pCreature->GetX(),
                        pCreature->GetY(), pCreature->GetZ(), pCreature->GetO(),
                        TEMPSUMMON_TIMED_DESPAWN, 5000);
                    pCreature->HandleEmoteCommand(EMOTE_ONESHOT_ATTACK1H);
                }
            }
        }

        m_bIsEventStarted = true;
        m_uiNextCheatTimer = 100000;
        m_nextCheat = CHEAT_FURY; // Always fury as first

        m_creature->GetMap()->PlayDirectSoundToMap(
            SAY_MEDIVH_START_GAME, m_creature->GetZoneId());
    }

    void NotifyOfKingDeath()
    {
        if (!m_pInstance)
            return;

        m_bIsEventStarted = false;
        m_KingLlane = ObjectGuid();
        m_WarchiefBlackhand = ObjectGuid();

        m_uiKingDeathTimer = 10000;

        for (auto& ref : m_pInstance->instance->GetPlayers())
            if (auto player = ref.getSource())
                player->remove_auras(SPELL_CONTROL_PIECE);

        if (m_pInstance->GetData(TYPE_CHESS) == DONE)
        {
            DoCastSpellIfCan(
                m_creature, SPELL_GAME_OVER_VISUAL, CAST_TRIGGERED);
            DoCastSpellIfCan(m_creature, SPELL_CAMERA_SHAKE, CAST_TRIGGERED);
            DoScriptText(EMOTE_WIN, m_creature);

            m_uiMaxVictoryTime = 10000;
            m_uinextVictoryRain = urand(400, 800);
            m_creature->GetMap()->PlayDirectSoundToMap(
                SAY_PLAYER_CHECKMATE, m_creature->GetZoneId());
        }
        else if (m_pInstance->GetData(TYPE_CHESS) == FAIL)
        {
            m_creature->GetMap()->PlayDirectSoundToMap(
                SAY_MEDIVH_CHECKMATE, m_creature->GetZoneId());
        }
    }

    void SpawnFuryNpcAndIgnite(Creature* pTarget)
    {
        if (!m_pInstance)
            return;

        Creature* pFury = m_creature->SummonCreature(NPC_FURY_OF_MEDIVH,
            pTarget->GetX(), pTarget->GetY(), pTarget->GetZ(), pTarget->GetO(),
            TEMPSUMMON_TIMED_DESPAWN, 60000);
        if (pFury)
        {
            pFury->CastSpell(pFury, SPELL_CHEAT_FURY, false);
            m_SpawnedFurys.push_back(pFury->GetObjectGuid());
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_pInstance)
            return;

        if (m_uinextVictoryRain)
        {
            if (m_uinextVictoryRain <= uiDiff)
            {
                uint32 space = urand(2, 30);
                // X: -0.2. Y: -4.0 == on the point between 4 spaces
                m_creature->SummonCreature(80005, EmptySpace[space].X - 0.2f,
                    EmptySpace[space].Y - 4.0f, EmptySpace[space].Z, 0.0f,
                    TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 15000);
                if (m_uiMaxVictoryTime != 0)
                    m_uinextVictoryRain = urand(200, 400);
                else
                    m_uinextVictoryRain = 0;
            }
            else
                m_uinextVictoryRain -= uiDiff;
        }

        if (m_uiMaxVictoryTime)
        {
            if (m_uiMaxVictoryTime <= uiDiff)
            {
                m_uiMaxVictoryTime = 0;
            }
            else
                m_uiMaxVictoryTime -= uiDiff;
        }

        if (m_uiKingDeathTimer)
        {
            if (m_uiKingDeathTimer <= uiDiff)
            {
                if (m_pInstance->GetData(TYPE_CHESS) != DONE)
                    m_uiRespawnBoardTimer = 10000;
                m_uiKingDeathTimer = 0;

                WipeBoard();
            }
            else
                m_uiKingDeathTimer -= uiDiff;
        }

        if (m_uiRespawnBoardTimer)
        {
            if (m_uiRespawnBoardTimer <= uiDiff)
            {
                SpawnBoard();
                m_uiRespawnBoardTimer = 0;
            }
            else
                m_uiRespawnBoardTimer -= uiDiff;
        }

        if (m_bIsEventStarted)
        {
            if (m_uiNextCheatTimer <= uiDiff)
            {
                if (m_nextCheat == CHEAT_HAND)
                {
                    std::vector<ObjectGuid> unitList;
                    ObjectGuid targetKing;
                    if (m_pInstance->GetData(DATA_CHESS_PLAYING_TEAM) ==
                        ALLIANCE)
                    {
                        targetKing = m_WarchiefBlackhand;
                        unitList.assign(
                            m_HordePieces.begin(), m_HordePieces.end());
                        if (unitList.size() > 0)
                            std::random_shuffle(unitList.begin(),
                                unitList.end(), medivh_shufflerng);
                    }
                    else if (m_pInstance->GetData(DATA_CHESS_PLAYING_TEAM) ==
                             HORDE)
                    {
                        targetKing = m_KingLlane;
                        unitList.assign(
                            m_AlliancePieces.begin(), m_AlliancePieces.end());
                        if (unitList.size() > 0)
                            std::random_shuffle(unitList.begin(),
                                unitList.end(), medivh_shufflerng);
                    }

                    if (unitList.size() > 0)
                    {
                        if (Creature* pKing =
                                m_creature->GetMap()->GetCreature(targetKing))
                            pKing->CastSpell(pKing, SPELL_CHEAT_HAND, true);
                        uint8 affectedTargets = 0;
                        for (auto& elem : unitList)
                        {
                            if (Creature* pCreature =
                                    m_creature->GetMap()->GetCreature(elem))
                            {
                                if (pCreature->isAlive() &&
                                    !pCreature->HasFlag(UNIT_FIELD_FLAGS,
                                        UNIT_FLAG_NON_ATTACKABLE) &&
                                    pCreature->GetObjectGuid() != targetKing)
                                {
                                    pCreature->CastSpell(
                                        pCreature, SPELL_CHEAT_HAND, true);
                                    ++affectedTargets;

                                    if (affectedTargets >= 2)
                                        break;
                                }
                            }
                        }
                    }
                }
                else if (m_nextCheat == CHEAT_FURY)
                {
                    std::vector<ObjectGuid> unitList;
                    ObjectGuid targetKing;
                    if (m_pInstance->GetData(DATA_CHESS_PLAYING_TEAM) ==
                        ALLIANCE)
                    {
                        targetKing = m_KingLlane;
                        unitList.assign(
                            m_AlliancePieces.begin(), m_AlliancePieces.end());
                        if (unitList.size() > 0)
                            std::random_shuffle(unitList.begin(),
                                unitList.end(), medivh_shufflerng);
                    }
                    else if (m_pInstance->GetData(DATA_CHESS_PLAYING_TEAM) ==
                             HORDE)
                    {
                        targetKing = m_WarchiefBlackhand;
                        unitList.assign(
                            m_HordePieces.begin(), m_HordePieces.end());
                        if (unitList.size() > 0)
                            std::random_shuffle(unitList.begin(),
                                unitList.end(), medivh_shufflerng);
                    }

                    if (unitList.size() > 0)
                    {
                        if (Creature* pKing =
                                m_creature->GetMap()->GetCreature(targetKing))
                            SpawnFuryNpcAndIgnite(pKing);
                        uint8 affectedTargets = 0;
                        for (auto& elem : unitList)
                        {
                            if (Creature* pCreature =
                                    m_creature->GetMap()->GetCreature(elem))
                            {
                                if (pCreature->isAlive() &&
                                    !pCreature->HasFlag(UNIT_FIELD_FLAGS,
                                        UNIT_FLAG_NON_ATTACKABLE) &&
                                    pCreature->GetObjectGuid() != targetKing)
                                {
                                    SpawnFuryNpcAndIgnite(pCreature);
                                    ++affectedTargets;

                                    if (affectedTargets >= 2)
                                        break;
                                }
                            }
                        }
                    }
                }

                switch (urand(0, 2))
                {
                case 0:
                    m_creature->GetMap()->PlayDirectSoundToMap(
                        SAY_MEDIVH_CHEAT_1, m_creature->GetZoneId());
                    break;
                case 1:
                    m_creature->GetMap()->PlayDirectSoundToMap(
                        SAY_MEDIVH_CHEAT_2, m_creature->GetZoneId());
                    break;
                case 2:
                    m_creature->GetMap()->PlayDirectSoundToMap(
                        SAY_MEDIVH_CHEAT_3, m_creature->GetZoneId());
                    break;
                }
                m_creature->MonsterTextEmote(
                    "Echo of Medivh cheats.", m_creature, true);
                m_nextCheat = (MedivhCheat)urand(0, 1);

                m_uiNextCheatTimer = urand(90000, 100000);
            }
            else
                m_uiNextCheatTimer -= uiDiff;
        }
    }
};

CreatureAI* GetAI_boss_chess_echo_of_medivh(Creature* pCreature)
{
    return new boss_chess_echo_of_medivhAI(pCreature);
}

bool GossipHello_npc_Chess_Piece(Player* pPlayer, Creature* pCreature,
    Team team, const char* str, bool isKing = false)
{
    pPlayer->PrepareGossipMenu(
        pCreature, pPlayer->GetDefaultGossipMenuForSource(pCreature));

    if (mob_chess_pieceAI* pieceAI =
            dynamic_cast<mob_chess_pieceAI*>(pCreature->AI()))
    {
        if (!pieceAI->m_pInstance)
            return true;
        bool canInitiateControl =
            (isKing) ?
                true :
                ((pieceAI->m_pInstance->GetData(TYPE_CHESS) == IN_PROGRESS) ?
                        true :
                        false);
        if (canInitiateControl && pPlayer->GetTeam() == team &&
            pieceAI->IsAvailableForControlBy(pPlayer))
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, str, GOSSIP_SENDER_MAIN,
                GOSSIP_ACTION_INFO_DEF);
    }

    pPlayer->TalkedToCreature(
        pCreature->GetEntry(), pCreature->GetObjectGuid());
    pPlayer->SendPreparedGossip(pCreature);

    return true;
}

bool GossipSelect_npc_Chess_Piece(Player* pPlayer, Creature* pCreature,
    uint32 /*uiSender*/, uint32 uiAction, Team team, bool isKing = false)
{
    if (uiAction == GOSSIP_ACTION_INFO_DEF)
    {
        if (mob_chess_pieceAI* pieceAI =
                dynamic_cast<mob_chess_pieceAI*>(pCreature->AI()))
        {
            if (!pieceAI->m_pInstance)
                return true;
            bool canInitiateControl =
                (isKing) ? true : ((pieceAI->m_pInstance->GetData(TYPE_CHESS) ==
                                       IN_PROGRESS) ?
                                          true :
                                          false);
            if (canInitiateControl && pPlayer->GetTeam() == team &&
                pieceAI->IsAvailableForControlBy(pPlayer))
            {
                pPlayer->CastSpell(pCreature, SPELL_CONTROL_PIECE, true);
                pPlayer->CLOSE_GOSSIP_MENU();
                pPlayer->NearTeleportTo(-11107.47f, -1842.63f, 229.63f, 5.37f);
                // Start event if we're king and it's not started:
                if (isKing &&
                    pieceAI->m_pInstance->GetData(TYPE_CHESS) != IN_PROGRESS)
                {
                    pieceAI->m_pInstance->SetData(TYPE_CHESS, IN_PROGRESS);
                    pieceAI->m_pInstance->SetData(
                        DATA_CHESS_PLAYING_TEAM, team);
                    if (Creature* pMedivh =
                            pieceAI->m_pInstance->GetSingleCreatureFromStorage(
                                NPC_MEDIVH))
                    {
                        if (boss_chess_echo_of_medivhAI* pMedAI =
                                dynamic_cast<boss_chess_echo_of_medivhAI*>(
                                    pMedivh->AI()))
                            pMedAI->StartEvent();
                    }
                }
            }
        }
    }

    return true;
}

/* SPECIFIC PIECES */

// King Llane
struct MANGOS_DLL_DECL mob_chess_king_llaneAI : public mob_chess_pieceAI
{
    mob_chess_king_llaneAI(Creature* pCreature) : mob_chess_pieceAI(pCreature)
    {
    }

    void Reset() override {}

    void JustDied(Unit* /*pKiller*/) override
    {
        if (!m_pInstance)
            return;
        if (m_pInstance->GetData(DATA_CHESS_PLAYING_TEAM) == ALLIANCE)
            m_pInstance->SetData(TYPE_CHESS, FAIL);
        else if (m_pInstance->GetData(DATA_CHESS_PLAYING_TEAM) == HORDE)
            m_pInstance->SetData(TYPE_CHESS, DONE);
        else
            m_pInstance->SetData(TYPE_CHESS, NOT_STARTED);

        if (Creature* pMedivh =
                m_pInstance->GetSingleCreatureFromStorage(NPC_MEDIVH))
        {
            if (boss_chess_echo_of_medivhAI* pAI =
                    dynamic_cast<boss_chess_echo_of_medivhAI*>(pMedivh->AI()))
            {
                pAI->NotifyOfKingDeath();
            }
        }
    }

    void AlertPieceOfGameStart() override
    {
        mob_chess_pieceAI::AlertPieceOfGameStart();
        if (urand(0, 1))
            m_uiMoveTimer = urand(30000, 60000);
        else
            m_uiMoveTimer = urand(2 * 60 * 1000, 3 * 60 * 1000);

        m_uiSpecialAttackTimer = 30000;

        m_uiHeroismCD = 45000;

        m_bHasSaidCheck = false;
    }

    void DamageTaken(Unit* pDealer, uint32& uiDamage) override
    {
        if (!m_pInstance)
            return;
        mob_chess_pieceAI::DamageTaken(pDealer, uiDamage);

        if (!m_bHasSaidCheck && m_creature->GetHealthPercent() < 20)
        {
            if (Creature* pMedivh =
                    m_pInstance->GetSingleCreatureFromStorage(NPC_MEDIVH))
            {
                if (m_teamPlaying == HORDE)
                    pMedivh->GetMap()->PlayDirectSoundToMap(
                        SAY_MEDIVH_CHECK, pMedivh->GetZoneId());
                else if (m_teamPlaying == ALLIANCE)
                    pMedivh->GetMap()->PlayDirectSoundToMap(
                        SAY_PLAYER_CHECK, pMedivh->GetZoneId());
            }
            m_bHasSaidCheck = true;
        }
    }

    bool m_bHasSaidCheck;
    uint32 m_uiHeroismCD;

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_pInstance)
            return;

        mob_chess_pieceAI::UpdateAI(uiDiff);

        // In case we're a doll:
        if (m_uiStartTimer)
        {
            if (m_uiStartTimer <= uiDiff)
            {
                m_uiStartTimer = 0;
            }
            else
            {
                m_uiStartTimer -= uiDiff;
                return;
            }
        }

        if (m_bIsJustADoll)
            return;

        if (!m_bIsGameStarted)
            return;

        if (m_bIsControlled)
            return;

        // Individual Piece AI:

        if (m_uiHeroismCD)
        {
            if (m_uiHeroismCD <= uiDiff)
                m_uiHeroismCD = 0;
            else
                m_uiHeroismCD -= uiDiff;
        }

        if (m_uiSpecialAttackTimer == 0)
        {
            if (Unit* pTarget = GetMeleeReachableTarget())
            {
                if (m_uiHeroismCD != 0 && urand(1, 100) <= 20)
                {
                    DoCastSpellIfCan(m_creature, 37471); // Heroism
                    m_uiSpecialAttackTimer = 5000;
                    m_uiHeroismCD = 15000;
                    return;
                }

                if (!pTarget->isInFront(pTarget, 8.0f, IN_FRONT_15_F))
                {
                    DoCastSpellIfCan(pTarget, SPELL_CHANGE_FACING);
                    m_uiMoveTimer += 6000;
                    m_uiSpecialAttackTimer = 1500;
                    return;
                }
                pTarget->SetInCombatWith(m_creature);
                m_creature->CastSpell(m_creature->GetX(), m_creature->GetY(),
                    m_creature->GetZ(), 37474, false);
                if (m_uiMoveTimer <= 6000)
                    m_uiMoveTimer = 6000;
            }
            m_uiSpecialAttackTimer = 5000;
        }

        if (m_teamPlaying != HORDE)
            return;

        if (m_uiMoveTimer == 0)
        {
            if (GetMeleeReachableTarget() != NULL)
            {
                m_uiMoveTimer = 30000;
                return;
            }

            if (Creature* pMedivh =
                    m_pInstance->GetSingleCreatureFromStorage(NPC_MEDIVH))
            {
                if (boss_chess_echo_of_medivhAI* pMedAI =
                        dynamic_cast<boss_chess_echo_of_medivhAI*>(
                            pMedivh->AI()))
                {
                    if (Creature* pCreature = m_creature->GetMap()->GetCreature(
                            pMedAI->m_WarchiefBlackhand))
                        if (!m_creature->HasInArc(IN_FRONT_F, pCreature))
                            DoCastSpellIfCan(pCreature, SPELL_CHANGE_FACING);
                }
            }

            if (Unit* pMove = GetMoveInFrontOfMe(9.0f, true))
            {
                if (DoCastSpellIfCan(pMove, SPELL_NORMAL_MOVE) == CAST_OK)
                    m_uiMoveTimer = urand(8000, 45000);
                else
                    m_uiMoveTimer = 2000; // Try again in 2
            }
            else
                m_uiMoveTimer = 5000; // Try again in 5
        }
    }
};
CreatureAI* GetAI_mob_chess_king_llane(Creature* pCreature)
{
    return new mob_chess_king_llaneAI(pCreature);
}
bool GossipHello_mob_chess_king_llane(Player* pPlayer, Creature* pCreature)
{
    return GossipHello_npc_Chess_Piece(
        pPlayer, pCreature, ALLIANCE, "Control King Llane", true);
}
bool GossipSelect_mob_chess_king_llane(
    Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction)
{
    return GossipSelect_npc_Chess_Piece(
        pPlayer, pCreature, uiSender, uiAction, ALLIANCE, true);
}

// Human Conjurer
struct MANGOS_DLL_DECL mob_chess_human_conjurerAI : public mob_chess_pieceAI
{
    mob_chess_human_conjurerAI(Creature* pCreature)
      : mob_chess_pieceAI(pCreature)
    {
    }

    uint32 m_uiRainTimer;

    void Reset() override {}

    void AlertPieceOfGameStart() override
    {
        mob_chess_pieceAI::AlertPieceOfGameStart();

        if (urand(0, 1))
            m_uiMoveTimer = urand(30000, 60000);
        else
            m_uiMoveTimer = urand(2 * 60 * 1000, 3 * 60 * 1000);

        m_uiSpecialAttackTimer = 30000;

        m_uiRainTimer = 20000;
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_pInstance)
            return;

        mob_chess_pieceAI::UpdateAI(uiDiff);

        // In case we're a doll:
        if (m_uiStartTimer)
        {
            if (m_uiStartTimer <= uiDiff)
            {
                m_uiStartTimer = 0;
            }
            else
            {
                m_uiStartTimer -= uiDiff;
                return;
            }
        }

        if (m_bIsJustADoll)
            return;

        if (!m_bIsGameStarted)
            return;

        if (m_bIsControlled)
            return;

        // Individual Piece AI:

        if (m_uiSpecialAttackTimer == 0)
        {
            if (Unit* pTarget = this->GetSpellReachableTarget())
            {
                if (!pTarget->isInFront(pTarget, 25.0f, IN_FRONT_15_F))
                {
                    DoCastSpellIfCan(pTarget, SPELL_CHANGE_FACING);
                    m_uiMoveTimer += 6000;
                    m_uiSpecialAttackTimer = 1500;
                    return;
                }
                pTarget->SetInCombatWith(m_creature);
                m_creature->CastSpell(pTarget, 37462, false);
                if (m_uiMoveTimer <= 6000)
                    m_uiMoveTimer = 6000;
            }
            m_uiSpecialAttackTimer = 5000;
        }

        if (m_uiRainTimer <= uiDiff)
        {
            if (Unit* pTarget = GetSpellReachableTarget())
            {
                m_uiRainTimer = 45000;
                m_creature->CastSpell(pTarget->GetX(), pTarget->GetY(),
                    pTarget->GetZ(), 37465, false);
            }
        }
        else
            m_uiRainTimer -= uiDiff;

        if (m_teamPlaying != HORDE)
            return;

        if (m_uiMoveTimer == 0)
        {
            if (GetSpellReachableTarget() != NULL)
            {
                m_uiMoveTimer = 30000;
                return;
            }

            if (Creature* pMedivh =
                    m_pInstance->GetSingleCreatureFromStorage(NPC_MEDIVH))
            {
                if (boss_chess_echo_of_medivhAI* pMedAI =
                        dynamic_cast<boss_chess_echo_of_medivhAI*>(
                            pMedivh->AI()))
                {
                    if (Creature* pCreature = m_creature->GetMap()->GetCreature(
                            pMedAI->m_WarchiefBlackhand))
                        if (!m_creature->HasInArc(IN_FRONT_F, pCreature))
                            DoCastSpellIfCan(pCreature, SPELL_CHANGE_FACING);
                }
            }

            if (Unit* pMove = GetMoveInFrontOfMe(20.0f, true))
            {
                if (DoCastSpellIfCan(pMove, SPELL_QUEEN_MOVE) == CAST_OK)
                    m_uiMoveTimer = urand(8000, 45000);
                else
                    m_uiMoveTimer = 2000; // Try again in 2
            }
            else
                m_uiMoveTimer = 5000; // Try again in 5
        }
    }
};
CreatureAI* GetAI_mob_chess_human_conjurer(Creature* pCreature)
{
    return new mob_chess_human_conjurerAI(pCreature);
}
bool GossipHello_mob_chess_human_conjurer(Player* pPlayer, Creature* pCreature)
{
    return GossipHello_npc_Chess_Piece(
        pPlayer, pCreature, ALLIANCE, "Control Human Conjurer");
}
bool GossipSelect_mob_chess_human_conjurer(
    Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction)
{
    return GossipSelect_npc_Chess_Piece(
        pPlayer, pCreature, uiSender, uiAction, ALLIANCE);
}

// Human Footman
struct MANGOS_DLL_DECL mob_chess_human_footmanAI : public mob_chess_pieceAI
{
    mob_chess_human_footmanAI(Creature* pCreature)
      : mob_chess_pieceAI(pCreature)
    {
    }

    void Reset() override {}

    void AlertPieceOfGameStart() override
    {
        mob_chess_pieceAI::AlertPieceOfGameStart();
        uint32 i = urand(1, 100);
        if (i <= 30) // Wait 3-5 minutes
        {
            m_uiMoveTimer = urand(3, 5) * 60 * 1000;
        }
        else if (i <= 90) // Normal start time
        {
            m_uiMoveTimer = urand(8000, 75000);
        }
        else // Instant start
        {
            m_uiMoveTimer = 8000;
        }

        m_uiSpecialAttackTimer = urand(15000, 25000);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        mob_chess_pieceAI::UpdateAI(uiDiff);

        // In case we're a doll:
        if (m_uiStartTimer)
        {
            if (m_uiStartTimer <= uiDiff)
            {
                m_uiStartTimer = 0;
            }
            else
            {
                m_uiStartTimer -= uiDiff;
                return;
            }
        }

        if (m_bIsJustADoll)
            return;

        if (!m_bIsGameStarted)
            return;

        if (m_bIsControlled)
            return;

        // Individual Piece AI:

        if (m_uiSpecialAttackTimer == 0)
        {
            if (Unit* pTarget = EnemyPieceInFrontOfMe())
            {
                pTarget->SetInCombatWith(m_creature);
                m_creature->CastSpell(m_creature->GetX(), m_creature->GetY(),
                    m_creature->GetZ(), 37406, false);
                if (m_uiMoveTimer <= 6000)
                    m_uiMoveTimer = 6000;
            }
            m_uiSpecialAttackTimer = 5000;
        }

        if (m_teamPlaying != HORDE)
            return;

        if (m_uiMoveTimer == 0)
        {
            if (EnemyPieceInFrontOfMe() != NULL)
                m_uiMoveTimer = urand(8000, 45000);

            if (Unit* pMove = GetMoveInFrontOfMe())
            {
                if (DoCastSpellIfCan(pMove, SPELL_NORMAL_MOVE) == CAST_OK)
                    m_uiMoveTimer = urand(8000, 45000);
                else
                    m_uiMoveTimer = 2000; // Try again in 2
            }
            else
                m_uiMoveTimer = 5000; // Try again in 5
        }
    }
};
CreatureAI* GetAI_mob_chess_human_footman(Creature* pCreature)
{
    return new mob_chess_human_footmanAI(pCreature);
}
bool GossipHello_mob_chess_human_footman(Player* pPlayer, Creature* pCreature)
{
    return GossipHello_npc_Chess_Piece(
        pPlayer, pCreature, ALLIANCE, "Control Human Footman");
}
bool GossipSelect_mob_chess_human_footman(
    Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction)
{
    return GossipSelect_npc_Chess_Piece(
        pPlayer, pCreature, uiSender, uiAction, ALLIANCE);
}

// Human Cleric
struct MANGOS_DLL_DECL mob_chess_human_clericAI : public mob_chess_pieceAI
{
    mob_chess_human_clericAI(Creature* pCreature) : mob_chess_pieceAI(pCreature)
    {
    }

    void Reset() override {}

    void AlertPieceOfGameStart() override
    {
        mob_chess_pieceAI::AlertPieceOfGameStart();

        if (urand(0, 1))
            m_uiMoveTimer = urand(8000, 45000);
        else
            m_uiMoveTimer = urand(1 * 60 * 1000, 2 * 60 * 1000);

        m_uiHealTimer = 20000;
    }

    uint32 m_uiHealTimer;

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_pInstance)
            return;
        mob_chess_pieceAI::UpdateAI(uiDiff);

        // In case we're a doll:
        if (m_uiStartTimer)
        {
            if (m_uiStartTimer <= uiDiff)
            {
                m_uiStartTimer = 0;
            }
            else
            {
                m_uiStartTimer -= uiDiff;
                return;
            }
        }

        if (m_bIsJustADoll)
            return;

        if (!m_bIsGameStarted)
            return;

        if (m_bIsControlled)
            return;

        if (m_uiHealTimer <= uiDiff)
        {
            if (Unit* pTarget = this->GetSpellReachableTarget(true, 25.0f, 80))
            {
                if (!pTarget->isInFront(pTarget, 30.0f, IN_FRONT_15_F))
                {
                    DoCastSpellIfCan(pTarget, SPELL_CHANGE_FACING);
                    m_uiMoveTimer += 2500;
                    m_uiHealTimer = 1500;
                    return;
                }
                m_creature->CastSpell(pTarget, 37455, false);
            }
            m_uiHealTimer = 20000;
        }
        else
            m_uiHealTimer -= uiDiff;

        if (m_teamPlaying != HORDE)
            return;

        if (m_uiMoveTimer == 0)
        {
            if (Creature* pMedivh =
                    m_pInstance->GetSingleCreatureFromStorage(NPC_MEDIVH))
            {
                if (boss_chess_echo_of_medivhAI* pMedAI =
                        dynamic_cast<boss_chess_echo_of_medivhAI*>(
                            pMedivh->AI()))
                {
                    // Are we close enough to our king to make a move?
                    if (Creature* pMyKing = m_creature->GetMap()->GetCreature(
                            pMedAI->m_KingLlane))
                    {
                        // If we're further than 17 yards away, but closer than
                        // 25, then we'd get OOR if we move now; let's wait
                        if (m_creature->GetDistance(pMyKing) > 17.0f &&
                            m_creature->GetDistance(pMyKing) < 25.0f)
                        {
                            m_uiMoveTimer = 20000;
                            return;
                        }
                    }

                    if (Creature* pCreature = m_creature->GetMap()->GetCreature(
                            pMedAI->m_WarchiefBlackhand))
                        if (!m_creature->HasInArc(IN_FRONT_F, pCreature))
                            DoCastSpellIfCan(pCreature, SPELL_CHANGE_FACING);
                }
            }

            if (Unit* pMove = GetMoveInFrontOfMe(9.0f, true))
            {
                if (DoCastSpellIfCan(pMove, SPELL_NORMAL_MOVE) == CAST_OK)
                    m_uiMoveTimer = urand(8000, 45000);
                else
                    m_uiMoveTimer = 2000; // Try again in 2
            }
            else
                m_uiMoveTimer = 5000; // Try again in 5
        }
    }
};
CreatureAI* GetAI_mob_chess_human_cleric(Creature* pCreature)
{
    return new mob_chess_human_clericAI(pCreature);
}
bool GossipHello_mob_chess_human_cleric(Player* pPlayer, Creature* pCreature)
{
    return GossipHello_npc_Chess_Piece(
        pPlayer, pCreature, ALLIANCE, "Control Human Cleric");
}
bool GossipSelect_mob_chess_human_cleric(
    Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction)
{
    return GossipSelect_npc_Chess_Piece(
        pPlayer, pCreature, uiSender, uiAction, ALLIANCE);
}

// Human Charger
struct MANGOS_DLL_DECL mob_chess_human_chargerAI : public mob_chess_pieceAI
{
    mob_chess_human_chargerAI(Creature* pCreature)
      : mob_chess_pieceAI(pCreature)
    {
    }

    void Reset() override {}

    void AlertPieceOfGameStart() override
    {
        mob_chess_pieceAI::AlertPieceOfGameStart();
        m_uiSpecialAttackTimer = urand(15000, 30000);
        m_uiMoveTimer = urand(30000, 120 * 1000);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_pInstance)
            return;
        mob_chess_pieceAI::UpdateAI(uiDiff);

        // In case we're a doll:
        if (m_uiStartTimer)
        {
            if (m_uiStartTimer <= uiDiff)
            {
                m_uiStartTimer = 0;
            }
            else
            {
                m_uiStartTimer -= uiDiff;
                return;
            }
        }

        if (m_bIsJustADoll)
            return;

        if (!m_bIsGameStarted)
            return;

        if (m_bIsControlled)
            return;

        // Individual Piece AI:

        if (m_uiSpecialAttackTimer == 0)
        {
            if (Unit* pTarget = GetMeleeReachableTarget())
            {
                if (!pTarget->isInFront(pTarget, 8.0f, IN_FRONT_15_F))
                {
                    DoCastSpellIfCan(pTarget, SPELL_CHANGE_FACING);
                    m_uiMoveTimer += 6000;
                    m_uiSpecialAttackTimer = 1500;
                    return;
                }
                pTarget->SetInCombatWith(m_creature);
                m_creature->CastSpell(m_creature->GetX(), m_creature->GetY(),
                    m_creature->GetZ(), 37453, false);
                if (m_uiMoveTimer <= 6000)
                    m_uiMoveTimer = 6000;
            }
            m_uiSpecialAttackTimer = 5000;
        }

        if (m_teamPlaying != HORDE)
            return;

        if (m_uiMoveTimer == 0)
        {
            if (GetMeleeReachableTarget() != NULL)
            {
                m_uiMoveTimer = 30000;
                return;
            }

            if (Creature* pMedivh =
                    m_pInstance->GetSingleCreatureFromStorage(NPC_MEDIVH))
            {
                if (boss_chess_echo_of_medivhAI* pMedAI =
                        dynamic_cast<boss_chess_echo_of_medivhAI*>(
                            pMedivh->AI()))
                {
                    if (Creature* pCreature = m_creature->GetMap()->GetCreature(
                            pMedAI->m_WarchiefBlackhand))
                        if (!m_creature->HasInArc(IN_FRONT_F, pCreature))
                            DoCastSpellIfCan(pCreature, SPELL_CHANGE_FACING);
                }
            }

            if (Unit* pMove = GetMoveInFrontOfMe(15.0f))
            {
                if (DoCastSpellIfCan(pMove, SPELL_HORSE_MOVE) == CAST_OK)
                    m_uiMoveTimer = urand(8000, 45000);
                else
                    m_uiMoveTimer = 2000; // Try again in 2
            }
            else
                m_uiMoveTimer = 5000; // Try again in 5
        }
    }
};
CreatureAI* GetAI_mob_chess_human_charger(Creature* pCreature)
{
    return new mob_chess_human_chargerAI(pCreature);
}
bool GossipHello_mob_chess_human_charger(Player* pPlayer, Creature* pCreature)
{
    return GossipHello_npc_Chess_Piece(
        pPlayer, pCreature, ALLIANCE, "Control Human Charger");
}
bool GossipSelect_mob_chess_human_charger(
    Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction)
{
    return GossipSelect_npc_Chess_Piece(
        pPlayer, pCreature, uiSender, uiAction, ALLIANCE);
}

// Conjured Water Elemental
struct MANGOS_DLL_DECL mob_chess_conjured_water_elementalAI
    : public mob_chess_pieceAI
{
    mob_chess_conjured_water_elementalAI(Creature* pCreature)
      : mob_chess_pieceAI(pCreature)
    {
    }

    void Reset() override {}

    void AlertPieceOfGameStart() override
    {
        mob_chess_pieceAI::AlertPieceOfGameStart();

        if (urand(0, 1))
            m_uiMoveTimer = urand(8000, 45000);
        else
            m_uiMoveTimer = urand(1 * 60 * 1000, 2 * 60 * 1000);

        m_uiSpecialAttackTimer = urand(30000, 40000);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_pInstance)
            return;
        mob_chess_pieceAI::UpdateAI(uiDiff);

        // In case we're a doll:
        if (m_uiStartTimer)
        {
            if (m_uiStartTimer <= uiDiff)
            {
                m_uiStartTimer = 0;
            }
            else
            {
                m_uiStartTimer -= uiDiff;
                return;
            }
        }

        if (m_bIsJustADoll)
            return;

        if (!m_bIsGameStarted)
            return;

        if (m_bIsControlled)
            return;

        // Individual Piece AI:

        if (m_uiSpecialAttackTimer == 0)
        {
            if (Unit* pTarget = GetMeleeReachableTarget())
            {
                if (!pTarget->isInFront(pTarget, 8.0f, IN_FRONT_15_F))
                {
                    DoCastSpellIfCan(pTarget, SPELL_CHANGE_FACING);
                    m_uiMoveTimer += 6000;
                    m_uiSpecialAttackTimer = 1500;
                    return;
                }
                pTarget->SetInCombatWith(m_creature);
                m_creature->CastSpell(m_creature->GetX(), m_creature->GetY(),
                    m_creature->GetZ(), 37427, false);
                if (m_uiMoveTimer <= 6000)
                    m_uiMoveTimer = 6000;
            }
            m_uiSpecialAttackTimer = 5000;
        }

        if (m_teamPlaying != HORDE)
            return;

        if (m_uiMoveTimer == 0)
        {
            if (GetMeleeReachableTarget() != NULL)
            {
                m_uiMoveTimer = 30000;
                return;
            }

            if (Creature* pMedivh =
                    m_pInstance->GetSingleCreatureFromStorage(NPC_MEDIVH))
            {
                if (boss_chess_echo_of_medivhAI* pMedAI =
                        dynamic_cast<boss_chess_echo_of_medivhAI*>(
                            pMedivh->AI()))
                {
                    if (Creature* pCreature = m_creature->GetMap()->GetCreature(
                            pMedAI->m_WarchiefBlackhand))
                        if (!m_creature->HasInArc(IN_FRONT_F, pCreature))
                            DoCastSpellIfCan(pCreature, SPELL_CHANGE_FACING);
                }
            }

            if (Unit* pMove = GetMoveInFrontOfMe(9.0f, true))
            {
                if (DoCastSpellIfCan(pMove, SPELL_NORMAL_MOVE) == CAST_OK)
                    m_uiMoveTimer = urand(8000, 45000);
                else
                    m_uiMoveTimer = 2000; // Try again in 2
            }
            else
                m_uiMoveTimer = 5000; // Try again in 5
        }
    }
};
CreatureAI* GetAI_mob_chess_conjured_water_elemental(Creature* pCreature)
{
    return new mob_chess_conjured_water_elementalAI(pCreature);
}
bool GossipHello_mob_conjured_water_elemental(
    Player* pPlayer, Creature* pCreature)
{
    return GossipHello_npc_Chess_Piece(
        pPlayer, pCreature, ALLIANCE, "Control Conjured Water Elemental");
}
bool GossipSelect_mob_conjured_water_elemental(
    Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction)
{
    return GossipSelect_npc_Chess_Piece(
        pPlayer, pCreature, uiSender, uiAction, ALLIANCE);
}

// Warchief Blackhand
struct MANGOS_DLL_DECL mob_chess_warchief_blackhandAI : public mob_chess_pieceAI
{
    mob_chess_warchief_blackhandAI(Creature* pCreature)
      : mob_chess_pieceAI(pCreature)
    {
    }

    void Reset() override {}

    void JustDied(Unit* /*pKiller*/) override
    {
        if (!m_pInstance)
            return;
        if (m_pInstance->GetData(DATA_CHESS_PLAYING_TEAM) == ALLIANCE)
            m_pInstance->SetData(TYPE_CHESS, DONE);
        else if (m_pInstance->GetData(DATA_CHESS_PLAYING_TEAM) == HORDE)
            m_pInstance->SetData(TYPE_CHESS, FAIL);
        else
            m_pInstance->SetData(TYPE_CHESS, NOT_STARTED);

        if (Creature* pMedivh =
                m_pInstance->GetSingleCreatureFromStorage(NPC_MEDIVH))
        {
            if (boss_chess_echo_of_medivhAI* pAI =
                    dynamic_cast<boss_chess_echo_of_medivhAI*>(pMedivh->AI()))
            {
                pAI->NotifyOfKingDeath();
            }
        }
    }

    void AlertPieceOfGameStart() override
    {
        mob_chess_pieceAI::AlertPieceOfGameStart();
        if (urand(0, 1))
            m_uiMoveTimer = urand(30000, 60000);
        else
            m_uiMoveTimer = urand(2 * 60 * 1000, 3 * 60 * 1000);

        m_uiSpecialAttackTimer = 30000;

        m_uiBloodlustCD = 45000;
        m_bHasSaidCheck = false;
    }

    void DamageTaken(Unit* pDealer, uint32& uiDamage) override
    {
        if (!m_pInstance)
            return;
        mob_chess_pieceAI::DamageTaken(pDealer, uiDamage);

        if (!m_bHasSaidCheck && m_creature->GetHealthPercent() < 20)
        {
            if (Creature* pMedivh =
                    m_pInstance->GetSingleCreatureFromStorage(NPC_MEDIVH))
            {
                if (m_teamPlaying == ALLIANCE)
                    pMedivh->GetMap()->PlayDirectSoundToMap(
                        SAY_MEDIVH_CHECK, pMedivh->GetZoneId());
                else if (m_teamPlaying == HORDE)
                    pMedivh->GetMap()->PlayDirectSoundToMap(
                        SAY_PLAYER_CHECK, pMedivh->GetZoneId());
            }
            m_bHasSaidCheck = true;
        }
    }

    bool m_bHasSaidCheck;
    uint32 m_uiBloodlustCD;

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_pInstance)
            return;
        mob_chess_pieceAI::UpdateAI(uiDiff);

        // In case we're a doll:
        if (m_uiStartTimer)
        {
            if (m_uiStartTimer <= uiDiff)
            {
                m_uiStartTimer = 0;
            }
            else
            {
                m_uiStartTimer -= uiDiff;
                return;
            }
        }

        if (m_bIsJustADoll)
            return;

        if (!m_bIsGameStarted)
            return;

        if (m_bIsControlled)
            return;

        // Individual Piece AI:

        if (m_uiBloodlustCD)
        {
            if (m_uiBloodlustCD <= uiDiff)
                m_uiBloodlustCD = 0;
            else
                m_uiBloodlustCD -= uiDiff;
        }

        if (m_uiSpecialAttackTimer == 0)
        {
            if (Unit* pTarget = GetMeleeReachableTarget())
            {
                if (m_uiBloodlustCD != 0 && urand(1, 100) <= 20)
                {
                    DoCastSpellIfCan(m_creature, 37472); // Bloodlust
                    m_uiSpecialAttackTimer = 5000;
                    m_uiBloodlustCD = 15000;
                    return;
                }

                if (!pTarget->isInFront(pTarget, 8.0f, IN_FRONT_15_F))
                {
                    DoCastSpellIfCan(pTarget, SPELL_CHANGE_FACING);
                    m_uiMoveTimer += 6000;
                    m_uiSpecialAttackTimer = 1500;
                    return;
                }
                pTarget->SetInCombatWith(m_creature);
                m_creature->CastSpell(m_creature->GetX(), m_creature->GetY(),
                    m_creature->GetZ(), 37476, false);
                if (m_uiMoveTimer <= 6000)
                    m_uiMoveTimer = 6000;
            }
            m_uiSpecialAttackTimer = 5000;
        }

        if (m_teamPlaying != ALLIANCE)
            return;

        if (m_uiMoveTimer == 0)
        {
            if (GetMeleeReachableTarget() != NULL)
            {
                m_uiMoveTimer = 30000;
                return;
            }

            if (Creature* pMedivh =
                    m_pInstance->GetSingleCreatureFromStorage(NPC_MEDIVH))
            {
                if (boss_chess_echo_of_medivhAI* pMedAI =
                        dynamic_cast<boss_chess_echo_of_medivhAI*>(
                            pMedivh->AI()))
                {
                    if (Creature* pCreature = m_creature->GetMap()->GetCreature(
                            pMedAI->m_KingLlane))
                        if (!m_creature->HasInArc(IN_FRONT_F, pCreature))
                            DoCastSpellIfCan(pCreature, SPELL_CHANGE_FACING);
                }
            }

            if (Unit* pMove = GetMoveInFrontOfMe(9.0f, true))
            {
                if (DoCastSpellIfCan(pMove, SPELL_NORMAL_MOVE) == CAST_OK)
                    m_uiMoveTimer = urand(8000, 45000);
                else
                    m_uiMoveTimer = 2000; // Try again in 2
            }
            else
                m_uiMoveTimer = 5000; // Try again in 5
        }
    }
};
CreatureAI* GetAI_mob_chess_warchief_blackhand(Creature* pCreature)
{
    return new mob_chess_warchief_blackhandAI(pCreature);
}
bool GossipHello_mob_chess_warchief_blackhand(
    Player* pPlayer, Creature* pCreature)
{
    return GossipHello_npc_Chess_Piece(
        pPlayer, pCreature, HORDE, "Control Warchief Blackhand", true);
}
bool GossipSelect_mob_chess_warchief_blackhand(
    Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction)
{
    return GossipSelect_npc_Chess_Piece(
        pPlayer, pCreature, uiSender, uiAction, HORDE, true);
}

// Orc Warlock
struct MANGOS_DLL_DECL mob_chess_orc_warlockAI : public mob_chess_pieceAI
{
    mob_chess_orc_warlockAI(Creature* pCreature) : mob_chess_pieceAI(pCreature)
    {
    }

    uint32 m_uiCloudTimer;

    void Reset() override {}

    void AlertPieceOfGameStart() override
    {
        mob_chess_pieceAI::AlertPieceOfGameStart();

        if (urand(0, 1))
            m_uiMoveTimer = urand(30000, 60000);
        else
            m_uiMoveTimer = urand(2 * 60 * 1000, 3 * 60 * 1000);

        m_uiSpecialAttackTimer = 30000;
        m_uiCloudTimer = 20000;
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_pInstance)
            return;
        mob_chess_pieceAI::UpdateAI(uiDiff);

        // In case we're a doll:
        if (m_uiStartTimer)
        {
            if (m_uiStartTimer <= uiDiff)
            {
                m_uiStartTimer = 0;
            }
            else
            {
                m_uiStartTimer -= uiDiff;
                return;
            }
        }

        if (m_bIsJustADoll)
            return;

        if (!m_bIsGameStarted)
            return;

        if (m_bIsControlled)
            return;

        // Individual Piece AI:

        if (m_uiSpecialAttackTimer == 0)
        {
            if (Unit* pTarget = this->GetSpellReachableTarget())
            {
                if (!pTarget->isInFront(pTarget, 25.0f, IN_FRONT_15_F))
                {
                    DoCastSpellIfCan(pTarget, SPELL_CHANGE_FACING);
                    m_uiMoveTimer += 6000;
                    m_uiSpecialAttackTimer = 1500;
                    return;
                }
                pTarget->SetInCombatWith(m_creature);
                m_creature->CastSpell(pTarget, 37463, false);
                if (m_uiMoveTimer <= 6000)
                    m_uiMoveTimer = 6000;
            }
            m_uiSpecialAttackTimer = 5000;
        }

        if (m_uiCloudTimer <= uiDiff)
        {
            if (Unit* pTarget = GetSpellReachableTarget())
            {
                m_uiCloudTimer = 45000;
                m_creature->CastSpell(pTarget->GetX(), pTarget->GetY(),
                    pTarget->GetZ(), 37469, false);
            }
        }
        else
            m_uiCloudTimer -= uiDiff;

        if (m_teamPlaying != ALLIANCE)
            return;

        if (m_uiMoveTimer == 0)
        {
            if (GetSpellReachableTarget() != NULL)
            {
                m_uiMoveTimer = 30000;
                return;
            }

            if (Creature* pMedivh =
                    m_pInstance->GetSingleCreatureFromStorage(NPC_MEDIVH))
            {
                if (boss_chess_echo_of_medivhAI* pMedAI =
                        dynamic_cast<boss_chess_echo_of_medivhAI*>(
                            pMedivh->AI()))
                {
                    if (Creature* pCreature = m_creature->GetMap()->GetCreature(
                            pMedAI->m_KingLlane))
                        if (!m_creature->HasInArc(IN_FRONT_F, pCreature))
                            DoCastSpellIfCan(pCreature, SPELL_CHANGE_FACING);
                }
            }

            if (Unit* pMove = GetMoveInFrontOfMe(20.0f, true))
            {
                if (DoCastSpellIfCan(pMove, SPELL_QUEEN_MOVE) == CAST_OK)
                    m_uiMoveTimer = urand(8000, 45000);
                else
                    m_uiMoveTimer = 2000; // Try again in 2
            }
            else
                m_uiMoveTimer = 5000; // Try again in 5
        }
    }
};
CreatureAI* GetAI_mob_chess_orc_warlock(Creature* pCreature)
{
    return new mob_chess_orc_warlockAI(pCreature);
}
bool GossipHello_mob_chess_orc_warlock(Player* pPlayer, Creature* pCreature)
{
    return GossipHello_npc_Chess_Piece(
        pPlayer, pCreature, HORDE, "Control Orc Warlock");
}
bool GossipSelect_mob_chess_orc_warlock(
    Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction)
{
    return GossipSelect_npc_Chess_Piece(
        pPlayer, pCreature, uiSender, uiAction, HORDE);
}

// Orc Grunt
struct MANGOS_DLL_DECL mob_chess_orc_gruntAI : public mob_chess_pieceAI
{
    mob_chess_orc_gruntAI(Creature* pCreature) : mob_chess_pieceAI(pCreature) {}

    void Reset() override {}

    void AlertPieceOfGameStart() override
    {
        mob_chess_pieceAI::AlertPieceOfGameStart();
        uint32 i = urand(1, 100);
        if (i <= 30) // Wait 3-5 minutes
        {
            m_uiMoveTimer = urand(3, 5) * 60 * 1000;
        }
        else if (i <= 90) // Normal start time
        {
            m_uiMoveTimer = urand(8000, 75000);
        }
        else // Instant start
        {
            m_uiMoveTimer = 8000;
        }

        m_uiSpecialAttackTimer = urand(15000, 25000);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        mob_chess_pieceAI::UpdateAI(uiDiff);

        // In case we're a doll:
        if (m_uiStartTimer)
        {
            if (m_uiStartTimer <= uiDiff)
            {
                m_uiStartTimer = 0;
            }
            else
            {
                m_uiStartTimer -= uiDiff;
                return;
            }
        }

        if (m_bIsJustADoll)
            return;

        if (!m_bIsGameStarted)
            return;

        if (m_bIsControlled)
            return;

        // Individual Piece AI:

        if (m_uiSpecialAttackTimer == 0)
        {
            if (Unit* pTarget = EnemyPieceInFrontOfMe())
            {
                pTarget->SetInCombatWith(m_creature);
                // DoCastSpellIfCan(NULL, 37413);
                m_creature->CastSpell(m_creature->GetX(), m_creature->GetY(),
                    m_creature->GetZ(), 37413, false);
                if (m_uiMoveTimer <= 6000)
                    m_uiMoveTimer = 6000;
            }
            m_uiSpecialAttackTimer = 5000;
        }

        if (m_teamPlaying != ALLIANCE)
            return;

        if (m_uiMoveTimer == 0)
        {
            if (EnemyPieceInFrontOfMe() != NULL)
                m_uiMoveTimer = urand(8000, 45000);

            if (Unit* pMove = GetMoveInFrontOfMe())
            {
                if (DoCastSpellIfCan(pMove, SPELL_NORMAL_MOVE) == CAST_OK)
                    m_uiMoveTimer = urand(8000, 45000);
                else
                    m_uiMoveTimer = 2000; // Try again in 2
            }
            else
                m_uiMoveTimer = 5000; // Try again in 5
        }
    }
};
CreatureAI* GetAI_mob_chess_orc_grunt(Creature* pCreature)
{
    return new mob_chess_orc_gruntAI(pCreature);
}
bool GossipHello_mob_chess_orc_grunt(Player* pPlayer, Creature* pCreature)
{
    return GossipHello_npc_Chess_Piece(
        pPlayer, pCreature, HORDE, "Control Orc Grunt");
}
bool GossipSelect_mob_chess_orc_grunt(
    Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction)
{
    return GossipSelect_npc_Chess_Piece(
        pPlayer, pCreature, uiSender, uiAction, HORDE);
}

// Orc Necrolyte
struct MANGOS_DLL_DECL mob_chess_orc_necrolyteAI : public mob_chess_pieceAI
{
    mob_chess_orc_necrolyteAI(Creature* pCreature)
      : mob_chess_pieceAI(pCreature)
    {
    }

    void Reset() override {}

    void AlertPieceOfGameStart() override
    {
        mob_chess_pieceAI::AlertPieceOfGameStart();

        if (urand(0, 1))
            m_uiMoveTimer = urand(8000, 45000);
        else
            m_uiMoveTimer = urand(1 * 60 * 1000, 2 * 60 * 1000);

        m_uiHealTimer = 20000;
    }

    uint32 m_uiHealTimer;

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_pInstance)
            return;
        mob_chess_pieceAI::UpdateAI(uiDiff);

        // In case we're a doll:
        if (m_uiStartTimer)
        {
            if (m_uiStartTimer <= uiDiff)
            {
                m_uiStartTimer = 0;
            }
            else
            {
                m_uiStartTimer -= uiDiff;
                return;
            }
        }

        if (m_bIsJustADoll)
            return;

        if (!m_bIsGameStarted)
            return;

        if (m_bIsControlled)
            return;

        if (m_uiHealTimer <= uiDiff)
        {
            if (Unit* pTarget = this->GetSpellReachableTarget(true, 25.0f, 80))
            {
                if (!pTarget->isInFront(pTarget, 30.0f, IN_FRONT_15_F))
                {
                    DoCastSpellIfCan(pTarget, SPELL_CHANGE_FACING);
                    m_uiMoveTimer += 2500;
                    m_uiHealTimer = 1500;
                    return;
                }
                m_creature->CastSpell(pTarget, 37456, false);
            }
            m_uiHealTimer = 20000;
        }
        else
            m_uiHealTimer -= uiDiff;

        if (m_teamPlaying != ALLIANCE)
            return;

        if (m_uiMoveTimer == 0)
        {
            if (Creature* pMedivh =
                    m_pInstance->GetSingleCreatureFromStorage(NPC_MEDIVH))
            {
                if (boss_chess_echo_of_medivhAI* pMedAI =
                        dynamic_cast<boss_chess_echo_of_medivhAI*>(
                            pMedivh->AI()))
                {
                    // Are we close enough to our king to make a move?
                    if (Creature* pMyKing = m_creature->GetMap()->GetCreature(
                            pMedAI->m_WarchiefBlackhand))
                    {
                        // If we're further than 17 yards away, but closer than
                        // 25, then we'd get OOR if we move now; let's wait
                        if (m_creature->GetDistance(pMyKing) > 17.0f &&
                            m_creature->GetDistance(pMyKing) < 25.0f)
                        {
                            m_uiMoveTimer = 20000;
                            return;
                        }
                    }

                    if (Creature* pCreature = m_creature->GetMap()->GetCreature(
                            pMedAI->m_KingLlane))
                        if (!m_creature->HasInArc(IN_FRONT_F, pCreature))
                            DoCastSpellIfCan(pCreature, SPELL_CHANGE_FACING);
                }
            }

            if (Unit* pMove = GetMoveInFrontOfMe(9.0f, true))
            {
                if (DoCastSpellIfCan(pMove, SPELL_NORMAL_MOVE) == CAST_OK)
                    m_uiMoveTimer = urand(8000, 45000);
                else
                    m_uiMoveTimer = 2000; // Try again in 2
            }
            else
                m_uiMoveTimer = 5000; // Try again in 5
        }
    }
};
CreatureAI* GetAI_mob_chess_orc_necrolyte(Creature* pCreature)
{
    return new mob_chess_orc_necrolyteAI(pCreature);
}
bool GossipHello_mob_chess_orc_necrolyte(Player* pPlayer, Creature* pCreature)
{
    return GossipHello_npc_Chess_Piece(
        pPlayer, pCreature, HORDE, "Control Orc Necrolyte");
}
bool GossipSelect_mob_chess_orc_necrolyte(
    Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction)
{
    return GossipSelect_npc_Chess_Piece(
        pPlayer, pCreature, uiSender, uiAction, HORDE);
}

// Orc Wolf
struct MANGOS_DLL_DECL mob_chess_orc_wolfAI : public mob_chess_pieceAI
{
    mob_chess_orc_wolfAI(Creature* pCreature) : mob_chess_pieceAI(pCreature) {}

    void Reset() override {}

    void AlertPieceOfGameStart() override
    {
        mob_chess_pieceAI::AlertPieceOfGameStart();
        m_uiSpecialAttackTimer = urand(15000, 30000);
        m_uiMoveTimer = urand(30000, 120 * 1000);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_pInstance)
            return;
        mob_chess_pieceAI::UpdateAI(uiDiff);

        // In case we're a doll:
        if (m_uiStartTimer)
        {
            if (m_uiStartTimer <= uiDiff)
            {
                m_uiStartTimer = 0;
            }
            else
            {
                m_uiStartTimer -= uiDiff;
                return;
            }
        }

        if (m_bIsJustADoll)
            return;

        if (!m_bIsGameStarted)
            return;

        if (m_bIsControlled)
            return;

        // Individual Piece AI:

        if (m_uiSpecialAttackTimer == 0)
        {
            if (Unit* pTarget = GetMeleeReachableTarget())
            {
                if (!pTarget->isInFront(pTarget, 8.0f, IN_FRONT_15_F))
                {
                    DoCastSpellIfCan(pTarget, SPELL_CHANGE_FACING);
                    m_uiMoveTimer += 6000;
                    m_uiSpecialAttackTimer = 1500;
                    return;
                }
                pTarget->SetInCombatWith(m_creature);
                m_creature->CastSpell(m_creature->GetX(), m_creature->GetY(),
                    m_creature->GetZ(), 37454, false);
                if (m_uiMoveTimer <= 6000)
                    m_uiMoveTimer = 6000;
            }
            m_uiSpecialAttackTimer = 5000;
        }

        if (m_teamPlaying != ALLIANCE)
            return;

        if (m_uiMoveTimer == 0)
        {
            if (GetMeleeReachableTarget() != NULL)
            {
                m_uiMoveTimer = 30000;
                return;
            }

            if (Creature* pMedivh =
                    m_pInstance->GetSingleCreatureFromStorage(NPC_MEDIVH))
            {
                if (boss_chess_echo_of_medivhAI* pMedAI =
                        dynamic_cast<boss_chess_echo_of_medivhAI*>(
                            pMedivh->AI()))
                {
                    if (Creature* pCreature = m_creature->GetMap()->GetCreature(
                            pMedAI->m_KingLlane))
                        if (!m_creature->HasInArc(IN_FRONT_F, pCreature))
                            DoCastSpellIfCan(pCreature, SPELL_CHANGE_FACING);
                }
            }

            if (Unit* pMove = GetMoveInFrontOfMe(15.0f))
            {
                if (DoCastSpellIfCan(pMove, SPELL_HORSE_MOVE) == CAST_OK)
                    m_uiMoveTimer = urand(8000, 45000);
                else
                    m_uiMoveTimer = 2000; // Try again in 2
            }
            else
                m_uiMoveTimer = 5000; // Try again in 5
        }
    }
};
CreatureAI* GetAI_mob_chess_orc_wolf(Creature* pCreature)
{
    return new mob_chess_orc_wolfAI(pCreature);
}
bool GossipHello_mob_chess_orc_wolf(Player* pPlayer, Creature* pCreature)
{
    return GossipHello_npc_Chess_Piece(
        pPlayer, pCreature, HORDE, "Control Orc Wolf");
}
bool GossipSelect_mob_chess_orc_wolf(
    Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction)
{
    return GossipSelect_npc_Chess_Piece(
        pPlayer, pCreature, uiSender, uiAction, HORDE);
}

// Summoned Daemon
struct MANGOS_DLL_DECL mob_chess_summoned_daemonAI : public mob_chess_pieceAI
{
    mob_chess_summoned_daemonAI(Creature* pCreature)
      : mob_chess_pieceAI(pCreature)
    {
    }

    void Reset() override {}

    void AlertPieceOfGameStart() override
    {
        mob_chess_pieceAI::AlertPieceOfGameStart();

        if (urand(0, 1))
            m_uiMoveTimer = urand(8000, 45000);
        else
            m_uiMoveTimer = urand(1 * 60 * 1000, 2 * 60 * 1000);

        m_uiSpecialAttackTimer = urand(30000, 40000);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_pInstance)
            return;
        mob_chess_pieceAI::UpdateAI(uiDiff);

        // In case we're a doll:
        if (m_uiStartTimer)
        {
            if (m_uiStartTimer <= uiDiff)
            {
                m_uiStartTimer = 0;
            }
            else
            {
                m_uiStartTimer -= uiDiff;
                return;
            }
        }

        if (m_bIsJustADoll)
            return;

        if (!m_bIsGameStarted)
            return;

        if (m_bIsControlled)
            return;

        // Individual Piece AI:

        if (m_uiSpecialAttackTimer == 0)
        {
            if (Unit* pTarget = GetMeleeReachableTarget())
            {
                if (!pTarget->isInFront(pTarget, 8.0f, IN_FRONT_15_F))
                {
                    DoCastSpellIfCan(pTarget, SPELL_CHANGE_FACING);
                    m_uiMoveTimer += 6000;
                    m_uiSpecialAttackTimer = 1500;
                    return;
                }
                pTarget->SetInCombatWith(m_creature);
                m_creature->CastSpell(m_creature->GetX(), m_creature->GetY(),
                    m_creature->GetZ(), 37428, false);
                if (m_uiMoveTimer <= 6000)
                    m_uiMoveTimer = 6000;
            }
            m_uiSpecialAttackTimer = 5000;
        }

        if (m_teamPlaying != ALLIANCE)
            return;

        if (m_uiMoveTimer == 0)
        {
            if (GetMeleeReachableTarget() != NULL)
            {
                m_uiMoveTimer = 30000;
                return;
            }

            if (Creature* pMedivh =
                    m_pInstance->GetSingleCreatureFromStorage(NPC_MEDIVH))
            {
                if (boss_chess_echo_of_medivhAI* pMedAI =
                        dynamic_cast<boss_chess_echo_of_medivhAI*>(
                            pMedivh->AI()))
                {
                    if (Creature* pCreature = m_creature->GetMap()->GetCreature(
                            pMedAI->m_KingLlane))
                        if (!m_creature->HasInArc(IN_FRONT_F, pCreature))
                            DoCastSpellIfCan(pCreature, SPELL_CHANGE_FACING);
                }
            }

            if (Unit* pMove = GetMoveInFrontOfMe(9.0f, true))
            {
                if (DoCastSpellIfCan(pMove, SPELL_NORMAL_MOVE) == CAST_OK)
                    m_uiMoveTimer = urand(8000, 45000);
                else
                    m_uiMoveTimer = 2000; // Try again in 2
            }
            else
                m_uiMoveTimer = 5000; // Try again in 5
        }
    }
};
CreatureAI* GetAI_mob_chess_summoned_daemon(Creature* pCreature)
{
    return new mob_chess_summoned_daemonAI(pCreature);
}
bool GossipHello_mob_chess_summoned_daemon(Player* pPlayer, Creature* pCreature)
{
    return GossipHello_npc_Chess_Piece(
        pPlayer, pCreature, HORDE, "Control Summoned Daemon");
}
bool GossipSelect_mob_chess_summoned_daemon(
    Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction)
{
    return GossipSelect_npc_Chess_Piece(
        pPlayer, pCreature, uiSender, uiAction, HORDE);
}

struct MANGOS_DLL_DECL chess_move_markerAI : public ScriptedAI
{
    chess_move_markerAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        Reset();
    }

    uint32 m_uiDespawnTimer;
    uint32 m_uiMoveMarkerTimer;

    void Reset() override
    {
        m_creature->CastSpell(m_creature, SPELL_RED_MARKER, true);
        m_uiDespawnTimer = 4000;
        m_uiMoveMarkerTimer = 1000;
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiMoveMarkerTimer)
        {
            if (m_uiMoveMarkerTimer <= uiDiff)
            {
                m_creature->CastSpell(m_creature, SPELL_MOVE_MARKER, true);
                m_uiMoveMarkerTimer = 0;
            }
            else
                m_uiMoveMarkerTimer -= uiDiff;
        }

        if (m_uiDespawnTimer <= uiDiff)
        {
            m_creature->ForcedDespawn();
            m_uiDespawnTimer = 4000;
        }
        else
            m_uiDespawnTimer -= uiDiff;
    }
};
CreatureAI* GetAI_chess_move_marker(Creature* pCreature)
{
    return new chess_move_markerAI(pCreature);
}

struct MANGOS_DLL_DECL chess_resurrection_visualAI : public ScriptedAI
{
    chess_resurrection_visualAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        Reset();
    }

    uint32 m_uiDespawnTimer;
    bool m_bDoneRes;

    void Reset() override
    {
        m_bDoneRes = false;
        m_uiDespawnTimer = 3000;
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_bDoneRes && m_uiDespawnTimer < 2500)
        {
            m_creature->CastSpell(m_creature, SPELL_RESURRECTION_VISUAL, true);
            m_bDoneRes = true;
        }

        if (m_uiDespawnTimer <= uiDiff)
        {
            m_creature->ForcedDespawn();
            m_uiDespawnTimer = 4000;
        }
        else
            m_uiDespawnTimer -= uiDiff;
    }
};
CreatureAI* GetAI_chess_resurrection_visual(Creature* pCreature)
{
    return new chess_resurrection_visualAI(pCreature);
}

void AddSC_chess_event()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "mob_chess_king_llane";
    pNewScript->GetAI = &GetAI_mob_chess_king_llane;
    pNewScript->pGossipHello = &GossipHello_mob_chess_king_llane;
    pNewScript->pGossipSelect = &GossipSelect_mob_chess_king_llane;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_chess_human_conjurer";
    pNewScript->GetAI = &GetAI_mob_chess_human_conjurer;
    pNewScript->pGossipHello = &GossipHello_mob_chess_human_conjurer;
    pNewScript->pGossipSelect = &GossipSelect_mob_chess_human_conjurer;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_chess_human_footman";
    pNewScript->GetAI = &GetAI_mob_chess_human_footman;
    pNewScript->pGossipHello = &GossipHello_mob_chess_human_footman;
    pNewScript->pGossipSelect = &GossipSelect_mob_chess_human_footman;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_chess_human_cleric";
    pNewScript->GetAI = &GetAI_mob_chess_human_cleric;
    pNewScript->pGossipHello = &GossipHello_mob_chess_human_cleric;
    pNewScript->pGossipSelect = &GossipSelect_mob_chess_human_cleric;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_chess_human_charger";
    pNewScript->GetAI = &GetAI_mob_chess_human_charger;
    pNewScript->pGossipHello = &GossipHello_mob_chess_human_charger;
    pNewScript->pGossipSelect = &GossipSelect_mob_chess_human_charger;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_chess_conjured_water_elemental";
    pNewScript->GetAI = &GetAI_mob_chess_conjured_water_elemental;
    pNewScript->pGossipHello = &GossipHello_mob_conjured_water_elemental;
    pNewScript->pGossipSelect = &GossipSelect_mob_conjured_water_elemental;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_chess_warchief_blackhand";
    pNewScript->GetAI = &GetAI_mob_chess_warchief_blackhand;
    pNewScript->pGossipHello = &GossipHello_mob_chess_warchief_blackhand;
    pNewScript->pGossipSelect = &GossipSelect_mob_chess_warchief_blackhand;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_chess_orc_warlock";
    pNewScript->GetAI = &GetAI_mob_chess_orc_warlock;
    pNewScript->pGossipHello = &GossipHello_mob_chess_orc_warlock;
    pNewScript->pGossipSelect = &GossipSelect_mob_chess_orc_warlock;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_chess_orc_grunt";
    pNewScript->GetAI = &GetAI_mob_chess_orc_grunt;
    pNewScript->pGossipHello = &GossipHello_mob_chess_orc_grunt;
    pNewScript->pGossipSelect = &GossipSelect_mob_chess_orc_grunt;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_chess_orc_necrolyte";
    pNewScript->GetAI = &GetAI_mob_chess_orc_necrolyte;
    pNewScript->pGossipHello = &GossipHello_mob_chess_orc_necrolyte;
    pNewScript->pGossipSelect = &GossipSelect_mob_chess_orc_necrolyte;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_chess_orc_wolf";
    pNewScript->GetAI = &GetAI_mob_chess_orc_wolf;
    pNewScript->pGossipHello = &GossipHello_mob_chess_orc_wolf;
    pNewScript->pGossipSelect = &GossipSelect_mob_chess_orc_wolf;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_chess_summoned_daemon";
    pNewScript->GetAI = &GetAI_mob_chess_summoned_daemon;
    pNewScript->pGossipHello = &GossipHello_mob_chess_summoned_daemon;
    pNewScript->pGossipSelect = &GossipSelect_mob_chess_summoned_daemon;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_chess_echo_of_medivh";
    pNewScript->GetAI = &GetAI_boss_chess_echo_of_medivh;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_chess_fury_cheat";
    pNewScript->GetAI = &GetAI_mob_chess_fury_cheat;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "chess_move_marker";
    pNewScript->GetAI = &GetAI_chess_move_marker;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "chess_resurrection_visual";
    pNewScript->GetAI = &GetAI_chess_resurrection_visual;
    pNewScript->RegisterSelf();
}
