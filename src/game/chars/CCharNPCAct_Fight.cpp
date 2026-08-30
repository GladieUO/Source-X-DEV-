// Actions specific to an NPC.

#include "../../common/sphere_library/CSRand.h"
#include "../../common/CLog.h"
//#include "../../common/CScriptParserBufs.h" // included in the precompiled header via CExpression.h
#include "../components/CCPropsItemWeapon.h"
#include "../items/item_types.h"
#include "../uo_files/uofiles_enums_creid.h"
#include "../CPathFinder.h"
#include "../triggers.h"
#include "CChar.h"
#include "CCharNPC.h"
#include "../CWorldGameTime.h"
#include "../items/CItem.h"
#include "../items/CItemBase.h"

//////////////////////////
// CChar

bool CChar::NPC_FightArchery(CChar * pChar)
{
    ADDTOCALLSTACK("CChar::NPC_FightArchery");
    ASSERT(m_pNPC);

    if (!g_Cfg.IsSkillFlag(Skill_GetActive(), SKF_RANGED))
        return false;

    int iMinDist = 0;
    int iMaxDist = 0;

    // determine how far we can shoot with this bow
    CItem *pWeapon = m_uidWeapon.ItemFind();
    if (pWeapon)
    {
        const CItemBase *pDef = pWeapon->Item_GetDef();
        if (pDef)
        {
            iMinDist = pDef->GetRangeL(); // may be 0 (valid)
            iMaxDist = pDef->GetRangeH(); // MUST be >1 for bows
        }
    }

    // if range is not set on the weapon, default to ini settings
    // fallback ONLY if weapon did not define MAX range
    if (iMaxDist <= 1)
    {
        iMaxDist = g_Cfg.m_iArcheryMaxDist;
    }
    if (!iMinDist)
        iMinDist = g_Cfg.m_iArcheryMinDist;

    int iDist = GetTopDist3D(pChar);
    if (iDist > iMaxDist)
    {
        NPC_Act_Follow(false, iMinDist, true);
        return true;
    }

    if (!CanSeeLOS(pChar))
        {
            NPC_Act_Follow(false, iMinDist, true);
            return true;
        }

    if (iDist > iMinDist)
        return true;		// always use archery if distant enough

    if (!g_Rand.GetVal(2))	// move away
    {
        // Move away
        NPC_Act_Follow(false, iMaxDist, true);
        return true;
    }

    // Fine here.
    return true;
}

CChar * CChar::NPC_FightFindBestTarget(const std::vector<CChar*>* pvExcludeList)
{
    ADDTOCALLSTACK("CChar::NPC_FightFindBestTarget");
    ASSERT(m_pNPC);

        // === PET TARGET LOCK ===
    // Pets must NOT auto-switch targets using threat logic
    // === PET TARGET LOCK (no threat switching, but allow reacquire) ===
    if (IsStatFlag(STATF_PET))
    {
        CChar *pTarget = m_Fight_Targ_UID.CharFind();

        if (pTarget && pTarget->Fight_IsAttackableState() && CanSeeLOS(pTarget))
            return pTarget;

        // If the locked target is invalid (dead / gone / unreachable),
        // THEN allow normal selection so the pet can recover.
        // Do NOT return nullptr here.
    }
    // Find the best target to attack, and switch to this
    // new target even if I'm already attacking someone.

    if (GetAttackersCount())
    {
        const bool fUseThreat = ((NPC_GetAiFlags() & NPC_AI_THREAT) != 0);
        int iClosest = INT32_MAX;
        int64 iBestThreat = INT64_MIN;
        int64 iCurrentThreat = 0;
        CChar *pChar = nullptr;
        CChar *pClosest = nullptr;
        CChar *pBestThreat = nullptr;
        SKILL_TYPE skillWeapon = Fight_GetWeaponSkill();
        CChar *pCurrentTarget = m_Fight_Targ_UID.CharFind();

        // Do NOT use iterators here, since in this loop the m_lastAttackers vector can be altered, and so the iterator, making it invalid
        //for (std::vector<LastAttackers>::iterator it = m_lastAttackers.begin(); it != m_lastAttackers.end(); ++it)
        for (size_t i = 0; i < m_lastAttackers.size(); )
        {
            LastAttackers &refAttacker = m_lastAttackers[i];
            pChar = CUID::CharFindFromUID(refAttacker.charUID);
            if (!pChar)
            {
                ++i;
                continue;
            }
            if (!pChar->Fight_IsAttackableState())
            {
                ++i;
                continue;
            }
            if (pvExcludeList)
            {
                if (pvExcludeList->cend() != std::find(pvExcludeList->cbegin(), pvExcludeList->cend(), pChar))
                {
                    ++i;
                    continue;
                }
            }

            if (refAttacker.ignore)
            {
                bool fIgnore = true;
                if (IsTrigUsed(TRIGGER_HITIGNORE))
                {
                    CScriptTriggerArgsPtr pScriptArgs = CScriptParserBufs::GetCScriptTriggerArgsPtr();
                    pScriptArgs->m_iN1 = fIgnore;
                    OnTrigger(CTRIG_HitIgnore, pScriptArgs, pChar);
                    fIgnore = pScriptArgs->m_iN1 ? true : false;
                }
                if (fIgnore)
                {
                    ++i;
                    continue;
                }
            }
            if (!pClosest)
                pClosest = pChar;

            int iDist = GetDist(pChar);
            /*if (iDist > GetVisualRange())     // does this cause a deadlock sometimes?
            {
                Attacker_Delete(i, false, ATTACKER_CLEAR_DISTANCE);
                //++i; // Do NOT increment here!
                if (m_lastAttackers.empty())
                    break;
                continue;
            } */
            if (g_Cfg.IsSkillFlag(skillWeapon, SKF_RANGED) && ((iDist < g_Cfg.m_iArcheryMinDist) || (iDist > g_Cfg.m_iArcheryMaxDist)))
            {
                ++i;
                continue;
            }
            if (!CanSeeLOS(pChar))
            {
                ++i;
                continue;
            }

            if (fUseThreat)
            {
                const int64 iThreat = (int64)refAttacker.threat;
                if (pChar == pCurrentTarget)
                    iCurrentThreat = iThreat;

                if (!pBestThreat || (iThreat > iBestThreat) ||
                    ((iThreat == iBestThreat) && (pChar == pCurrentTarget)) ||
                    ((iThreat == iBestThreat) && (pBestThreat != pCurrentTarget) && (iDist < GetDist(pBestThreat))))
                {
                    pBestThreat = pChar;
                    iBestThreat = iThreat;
                }
            }

            if (iDist < iClosest)	// this char is more closer to me than my current target, let's switch to this target
            {
                pClosest = pChar;
                iClosest = iDist;
            }
            ++i;
        }
        if (fUseThreat && pBestThreat)
        {
            if (!pCurrentTarget)
                return pBestThreat;

            // If already targeting best, keep it
            if (pBestThreat == pCurrentTarget)
                return pCurrentTarget;

            // Keep the current target until another eligible target exceeds it by 20%.
            const int SWITCH_PCT = 120; // 120% = must exceed by 20%

            // Avoid division (safe integer math)
            if (iBestThreat * 100 >= iCurrentThreat * SWITCH_PCT)
                return pBestThreat;

            // Otherwise, keep current target
            return pCurrentTarget;
        }
        if (pClosest)
            return pClosest;
    }

    // New target not found, return the current target, if any
    CChar *pTarget = m_Fight_Targ_UID.CharFind();
    if (pTarget)
    {
        if (!pvExcludeList || (pvExcludeList->cend() == std::find(pvExcludeList->cbegin(), pvExcludeList->cend(), pTarget)))
            return pTarget;
    }

    return nullptr;
}

void CChar::NPC_Act_Fight()
{
    ADDTOCALLSTACK("CChar::NPC_Act_Fight");
    ASSERT(m_pNPC);

    // I am in an attack mode.
    // While casting (SKF_MAGIC), Fight_IsActive() returns false because casting skills are not SKF_FIGHT.
    // Keep combat state during spellcasting, otherwise NPCs clear fight data right after starting a spell.
    const SKILL_TYPE iActiveSkill = Skill_GetActive();
    CChar *pCombatTarget          = m_Fight_Targ_UID.CharFind();
    const bool fHasCombatTarget   = (pCombatTarget != nullptr);
    const bool fCastingInCombat   = g_Cfg.IsSkillFlag(iActiveSkill, SKF_MAGIC) && IsStatFlag(STATF_WAR) && fHasCombatTarget;
    const bool fPendingCombat     = IsStatFlag(STATF_WAR) && fHasCombatTarget;
    if (!Fight_IsActive() && !fCastingInCombat && !fPendingCombat)
    {
        Fight_ClearAll();
        return;
    }

    // Review our targets periodically.
    /* The code predates Sphere 56b and is used to find a new target. On the other hand, in combat, it is probably unnecessary anymore based on the details as follows;
    * Switching the target and movement is handled by NPC_FightFindBestTarget() in the combat routines.
    * It also break ups, some time, the NPC_ActFight trigger is all the condition were true.
    if (!IsStatFlag(STATF_PET) || (m_pNPC->m_Brain == NPCBRAIN_BERSERK))
    {
        int iObservant = (130 - Stat_GetAdjusted(STAT_INT)) / 20;
        if (!g_Rand.GetVal(2 + maximum(0, iObservant)))
        {
            if (NPC_LookAround())
            {
                _SetTimeoutD(5); // half of a second until the next check
                return;
            }
        }
    }
    */
    CChar *pChar = pCombatTarget;
    if (pChar == nullptr || !pChar->IsTopLevel()) // target is not valid anymore ?
    {
        pChar = NPC_FightFindBestTarget();
        if (pChar == nullptr || !pChar->IsTopLevel())
        {
            Fight_ClearAll();
            return;
        }
        m_Fight_Targ_UID = pChar->GetUID();
    }

    if (!Fight_IsActive() && !g_Cfg.IsSkillFlag(iActiveSkill, SKF_MAGIC))
    {
        Fight_Attack(pChar);
        return;
    }

    // If the current target cannot be attacked anymore, find a better one
    if (!pChar->Fight_IsAttackableState() || !CanSeeLOS(pChar))
    {
        int64 now = CWorldGameTime::GetCurrentTime().GetTimeRaw();

        if (!m_timeTargetLostLOS)
            m_timeTargetLostLOS = now;

        const int TARGET_LOS_TIMEOUT = 5000; // 5 seconds

        if (now - m_timeTargetLostLOS > TARGET_LOS_TIMEOUT)
        {
            CChar *pNew = NPC_FightFindBestTarget();

            if (pNew && pNew != pChar)
            {
                Fight_Attack(pNew);
                pChar = pNew;
            }
            else
            {
                if (IsStatFlag(STATF_PET))
                {
                    // Try to reach target instead of giving up
                    NPC_Act_Follow(false, 1, true);
                    return;
                }
                Skill_Start(SKILL_NONE);
                StatFlag_Clear(STATF_WAR);
                m_Fight_Targ_UID.InitUID();

                NPC_Act_Idle();
                return;
            }

            m_timeTargetLostLOS = 0;
        }
    }
    else
    {
        m_timeTargetLostLOS = 0;
    }

    if (Attacker_GetIgnore(pChar))
    {
        if (!NPC_FightFindBestTarget())
        {
            Skill_Start(SKILL_NONE);
            StatFlag_Clear(STATF_WAR);
            m_Fight_Targ_UID.InitUID();
            return;
        }
    }
    int iDist = GetDist(pChar);

    if ((m_pNPC->m_Brain == NPCBRAIN_GUARD) &&
        (m_atFight.m_iWarSwingState == WAR_SWING_READY) &&
        !g_Rand.Get16ValFast(3))
    {
        // If a guard is ever too far away (missed a chance to swing)
        // Teleport me closer.
        NPC_LookAtChar(pChar, iDist);
    }

    // If i'm horribly damaged and smart enough to know it.
    int iMotivation = NPC_GetAttackMotivation(pChar);

    bool fSkipHardcoded = false;
    if (IsTrigUsed(TRIGGER_NPCACTFIGHT))
    {
        CScriptTriggerArgsPtr pScriptArgs = CScriptParserBufs::GetCScriptTriggerArgsPtr();
        pScriptArgs->Init(iDist, iMotivation, 0, nullptr);
        switch (OnTrigger(CTRIG_NPCActFight, pScriptArgs, pChar))
        {
        case TRIGRET_RET_TRUE:
            return;
        case TRIGRET_RET_FALSE:
            fSkipHardcoded = true;
            break;
        case TRIGRET_RET_DEFAULT: //(TRIGRET_TYPE)(2) :
        {
            SKILL_TYPE iSkillforced = (SKILL_TYPE)ResGetIndex((dword)pScriptArgs->m_VarsLocal.GetKeyNum("skill"));
            if (iSkillforced)
            {
                SPELL_TYPE iSpellforced = (SPELL_TYPE)ResGetIndex((dword)pScriptArgs->m_VarsLocal.GetKeyNum("spell"));
                if (g_Cfg.IsSkillFlag(iSkillforced, SKF_MAGIC))
                {
                    m_atMagery.m_iSpell = iSpellforced;
                    m_Act_UID = m_Fight_Targ_UID; // Setting the spell's target.
                }

                Skill_Start(iSkillforced);
                return;
            }
        }
        default:
            break;
        }

        iDist = (int)(pScriptArgs->m_iN1);
        iMotivation = (int)(pScriptArgs->m_iN2);
    }

    if (!IsStatFlag(STATF_PET))
    {
        if (iMotivation < 0)
        {
            m_atFlee.m_iStepsMax = 20;	// how long should it take to get there.
            m_atFlee.m_iStepsCurrent = 0;	// how long has it taken ?
            if (NPC_Act_Flee())
            {
                Skill_Start(NPCACT_FLEE);	// Run away!
            }
            else
            {
                //We have to clean STATF_WAR if npc motivation below zero but can't see the target or flee target is invalid.
                Skill_Start(SKILL_NONE);
                StatFlag_Clear(STATF_WAR);
                Attacker_Delete(pChar, true, ATTACKER_CLEAR_DISTANCE);
                m_Fight_Targ_UID.InitUID();
            }
            return;
        }
    }

    // Can only do that with full stamina !
    if (!fSkipHardcoded && (Stat_GetVal(STAT_DEX) >= Stat_GetAdjusted(STAT_DEX)))
    {
        // If I am a dragon maybe I will breath fire.
        // NPCACT_BREATH
        if (m_pNPC->m_Brain == NPCBRAIN_DRAGON &&
            iDist >= 1 &&
            iDist <= 8 &&
            CanSeeLOS(pChar, LOS_NB_WINDOWS)) //Dragon can breath through a window
        {
            if (!IsSetCombatFlags(COMBAT_NODIRCHANGE))
                UpdateDir(pChar);
            Skill_Start(NPCACT_BREATH);
            return;
        }

        // any special ammunition defined?

        //check Range
        int iRangeMin = 2;
        int iRangeMax = 9;
        const CVarDefCont * pRange = GetDefKey("THROWRANGE", true);
        if (pRange)
        {
            int iRangeTot = CBaseBaseDef::ConvertRangeStr(pRange->GetValStr());
            iRangeMin = RANGE_GET_LO(iRangeTot);
            iRangeMax = RANGE_GET_HI(iRangeTot);
        }

        if (iDist >= iRangeMin && iDist <= iRangeMax && CanSeeLOS(pChar, LOS_NB_WINDOWS))//NPCs can throw through a window
        {
            const CVarDefCont * pRock = GetDefKey("THROWOBJ", true);
            const CREID_TYPE iDispID = GetDispID();
            if (iDispID == CREID_OGRE || iDispID == CREID_ETTIN || iDispID == CREID_CYCLOPS || pRock)
            {
                ITEMID_TYPE id = ITEMID_NOTHING;
                if (pRock)
                {
                    lpctstr t_Str = pRock->GetValStr();
                    CResourceID rid = g_Cfg.ResourceGetID(RES_ITEMDEF, t_Str);
                    ITEMID_TYPE obj = (ITEMID_TYPE)(rid.GetResIndex());
                    if (ContentFind(CResourceID(RES_ITEMDEF, obj), 0, 2))
                        id = ITEMID_NODRAW;
                }
                else
                {
                    if (ContentFind(CResourceID(RES_TYPEDEF, IT_AROCK), 0, 2))
                        id = ITEMID_NODRAW;
                }


                if (id != ITEMID_NOTHING)
                {
                    if (!IsSetCombatFlags(COMBAT_NODIRCHANGE))
                        UpdateDir(pChar);
                    Skill_Start(NPCACT_THROWING);
                    return;
                }
            }
        }
    }

    // Maybe i'll cast a spell if I can. if so maintain a distance.
    if (NPC_FightMagery(pChar))
    {
        return;
    }

    if (NPC_FightArchery(pChar))
    {
        return;
    }

    // Move in for melee type combat.
    // Only skilled melee fighters are allowed to close in
    if (Skill_GetBase(SKILL_TACTICS) > 0)
    {
        int iRange = Fight_CalcRange(m_uidWeapon.ItemFind());
        if (!NPC_Act_Follow(false, iRange, false))
        {
            m_Act_UID.InitUID();
            _SetTimeoutD(1);
            return;
        }
    }
    else
    {
        // Not a melee fighter → do NOT close in
        const int iMinRange2  = 4;
        const int iMaxRange2 = g_Cfg.m_iMaxSpellRange;
        if (iDist < iMinRange2)
            NPC_Act_Follow(false, 6, true); // back off slightly
        else if (iDist > iMaxRange2)
            // Too far → move closer
            NPC_Act_Follow(false, iMinRange2, false);
    }

    if (!_IsTimerSet()) // Nothing could be done, tick again in a while
    {
        NPC_LookAround();
        if (!_IsTimerSet())
        {
            DEBUG_MSG(("%s [0x%04x] found nothing to do in the fight routines.\n", GetName(), (dword)GetUID()));
            _SetTimeoutS(1);
        }
    }
}
