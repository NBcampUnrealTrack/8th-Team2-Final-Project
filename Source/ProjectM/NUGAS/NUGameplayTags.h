// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace NU
{
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(None);

    // 능력
    namespace Ability
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dead);
        
        namespace Field
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact);
        }

        namespace Battle
        {
            // 공용 행동 어빌리티
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(BasicAttack);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(UseItem);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Flee);
            
            // 액티브 스킬 공용 태그
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(ActiveSkill);
            
            // 나이아 전용 Ability
            namespace Naia
            {
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(SelfHeal);
            }
            
            // 적 전용 Ability
            namespace Enemy
            {
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(Telegraph);   // 큰 기술 앞에 한 턴 쉬며 경고만 함
            }
            
            namespace Character
            {
                
            }
        }
    }
    
    // 이벤트
    namespace Event
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dead);
        
        namespace Battle
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(BasicAttack);
        }
    }

    // 장소
    namespace Context
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Field);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Battle);
    }

    // 상태
    namespace State
    {
        namespace Field
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interacting);
        }

        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dead);           // 사망
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Parried);        // 패링 성공
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dodging);        // 회피 성공, SP를 사용해서 패링 불가 공격을 무효화함
        
        // 상태이상 태그, 아무 상태이상을 가지고 있는가를 확인할 수 있음
        namespace Ailment
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root);
            
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Burn);           // 화상
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Explosion);      // 폭발
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slow);           // 둔화
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Wetness);        // 젖음
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Poison);         // 중독
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hallucination);  // 환각
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Electrocution);  // 감전
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Paralysis);      // 마비
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stun);           // 기절
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fracture);       // 균열
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Blind);          // 실명
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fatal);          // 치명
            
            // 아무 상태이상 하나라도 걸려있는지 확인
            inline bool HasAnyAilment(const FGameplayTagContainer& Tags)
            {
                return Tags.HasTag(Root);
            }
        }
        
    }
    
    // 데미지 관련
    namespace Damage
    {
        // 데미지 타입 : 물리,마법
        namespace Type
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Physical);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Magical);
        }

        // 계수가 무엇에 비례하는지, 안 붙이면 기본값은 공격력, Type이 마법이면 마법 공격력
        namespace Base
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(MaxHealth);
        }
    }
    
    // 속성
    namespace Element
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root);
        
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fire);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Water);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Grass);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Lightning);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Earth);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Wind);
    }
    
    // 데이터 구분
    namespace Data
    {
        namespace Initial
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(MaxHealth);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Health);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(MaxSP);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(SP);
            
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackPower);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(MagicAttackPower);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Defense);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Speed);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(CriticalRate);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(CriticalDamage);
        }
        
        namespace Damage
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Coefficient);        // 데미지 계수
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(DefenseIgnore);      // 방어력 무시 비율
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Lifesteal);          // 흡혈 비율
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(AdditiveBonus);      // 합연산 풀에 더할 이 행동만의 값
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Multiplier);         // 곱연산 항 하나, 조건부 대폭 증가
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(ForceCritical);      // 1 이면 치명타를 굴리지 않고 치명타로 적중
        }
        
        namespace Skill
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Heal);               // 회복량
        }

        namespace Recovery
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Health);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(SP);
        }
        
        namespace StatusEffect
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(TickDamage);
            
            // 배율 변경 (곱)
            namespace Multiplier
            {
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(Speed);
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(Defense);
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(CriticalRate);
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(CriticalDamage);
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackPower);
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(MagicAttackPower);
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(MaxHealth);
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(MaxSP);
            }

            // 고정값 변경 (합) 
            namespace Additive
            {
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(Speed);
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(Defense);
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(CriticalRate);
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(CriticalDamage);
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackPower);
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(MagicAttackPower);
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(MaxHealth);
                UE_DECLARE_GAMEPLAY_TAG_EXTERN(MaxSP);
            }
        }
    }

    // 액터 머리 위 마커 아이콘 종류
    namespace Marker
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(QuestOffer);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(QuestLocked);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(QuestInProgress);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(QuestTurnIn);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(QuestTarget);
    }

    // 상호작용 선택지
    namespace Interaction
    {
        namespace Option
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dialogue);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Quest);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Job);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(NaiaContract);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shop);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Storage);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Recovery);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Crafting);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Encounter);
        }
    }

    // 퀘스트
    namespace Quest
    {
        // 퀘스트 분류
        namespace Category
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Main);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Side);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Daily);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tutorial);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Job);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Craft);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hidden);
        }

        // 퀘스트 시작 조건
        namespace Requirement
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(PlayerLevel);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(QuestCompleted);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(QuestGroupCompletedCount);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(FlagSet);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(FlagNotSet);
        }

        // 퀘스트 목표
        namespace Objective
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Talk);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(UseObject);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(CollectItem);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(PurchaseItem);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(UseStorage);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(EnemyDefeated);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(BattleWon);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(ReachLocation);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(GrowNaia);
        }

        // 퀘스트 보상
        namespace Reward
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Experience);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Gold);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetFlag);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(UnlockFeature);
        }
    }
    
    // 적 패턴
    namespace Enemy
    {
        // 보스 페이즈, 적 패턴 표의 PhaseTag 가 고름
        namespace Phase
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(First);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Second);
        }
    }
}

namespace GameplayCue
{
    // 전투에서 사용할 GameplayCue
    namespace Battle
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hit);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Heal);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(SPRestore);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Death);
    }
    
}
