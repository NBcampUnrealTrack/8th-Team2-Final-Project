// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/NUSFXTags.h"

namespace NU
{
	namespace SFX
	{
		namespace UI
		{
			UE_DEFINE_GAMEPLAY_TAG(Click, "NU.SFX.UI.Click")
			UE_DEFINE_GAMEPLAY_TAG(Hover, "NU.SFX.UI.Hover")
			UE_DEFINE_GAMEPLAY_TAG(Cursor, "NU.SFX.UI.Cursor")
			UE_DEFINE_GAMEPLAY_TAG(Confirm, "NU.SFX.UI.Confirm")
			UE_DEFINE_GAMEPLAY_TAG(Cancel, "NU.SFX.UI.Cancel")
			UE_DEFINE_GAMEPLAY_TAG(Error, "NU.SFX.UI.Error")
			UE_DEFINE_GAMEPLAY_TAG(Close, "NU.SFX.UI.Close")
			UE_DEFINE_GAMEPLAY_TAG(WindowClose, "NU.SFX.UI.WindowClose")
			UE_DEFINE_GAMEPLAY_TAG(WindowOpen, "NU.SFX.UI.WindowOpen")
			UE_DEFINE_GAMEPLAY_TAG(ModalOpen, "NU.SFX.UI.ModalOpen")
			UE_DEFINE_GAMEPLAY_TAG(PopupOpen, "NU.SFX.UI.PopupOpen")
			UE_DEFINE_GAMEPLAY_TAG(Tab, "NU.SFX.UI.Tab")
			UE_DEFINE_GAMEPLAY_TAG(Notice, "NU.SFX.UI.Notice")
			UE_DEFINE_GAMEPLAY_TAG(Step, "NU.SFX.UI.Step")
			UE_DEFINE_GAMEPLAY_TAG(MapOpen, "NU.SFX.UI.MapOpen")
			UE_DEFINE_GAMEPLAY_TAG(MapClose, "NU.SFX.UI.MapClose")
			UE_DEFINE_GAMEPLAY_TAG(ChestOpen, "NU.SFX.UI.ChestOpen")
			UE_DEFINE_GAMEPLAY_TAG(ChestClose, "NU.SFX.UI.ChestClose")
		}

		namespace Item
		{
			UE_DEFINE_GAMEPLAY_TAG(PickUp, "NU.SFX.Item.PickUp")
			UE_DEFINE_GAMEPLAY_TAG(Drop, "NU.SFX.Item.Drop")
			UE_DEFINE_GAMEPLAY_TAG(Unequip, "NU.SFX.Item.Unequip")
			UE_DEFINE_GAMEPLAY_TAG(Equip, "NU.SFX.Item.Equip")
			UE_DEFINE_GAMEPLAY_TAG(Use, "NU.SFX.Item.Use")
			UE_DEFINE_GAMEPLAY_TAG(Discard, "NU.SFX.Item.Discard")
			UE_DEFINE_GAMEPLAY_TAG(Sort, "NU.SFX.Item.Sort")
			UE_DEFINE_GAMEPLAY_TAG(Acquire, "NU.SFX.Item.Acquire")
		}

		namespace Shop
		{
			UE_DEFINE_GAMEPLAY_TAG(Buy, "NU.SFX.Shop.Buy")
			UE_DEFINE_GAMEPLAY_TAG(Sell, "NU.SFX.Shop.Sell")
			UE_DEFINE_GAMEPLAY_TAG(Craft, "NU.SFX.Shop.Craft")
		}

		namespace Trade
		{
			UE_DEFINE_GAMEPLAY_TAG(Invite, "NU.SFX.Trade.Invite")
			UE_DEFINE_GAMEPLAY_TAG(Offer, "NU.SFX.Trade.Offer")
			UE_DEFINE_GAMEPLAY_TAG(Lock, "NU.SFX.Trade.Lock")
			UE_DEFINE_GAMEPLAY_TAG(Complete, "NU.SFX.Trade.Complete")
			UE_DEFINE_GAMEPLAY_TAG(Cancel, "NU.SFX.Trade.Cancel")
		}

		namespace Social
		{
			UE_DEFINE_GAMEPLAY_TAG(PartyInvite, "NU.SFX.Social.PartyInvite")
			UE_DEFINE_GAMEPLAY_TAG(Chat, "NU.SFX.Social.Chat")
		}

		namespace Char
		{
			UE_DEFINE_GAMEPLAY_TAG(Select, "NU.SFX.Char.Select")
			UE_DEFINE_GAMEPLAY_TAG(Create, "NU.SFX.Char.Create")
			UE_DEFINE_GAMEPLAY_TAG(Delete, "NU.SFX.Char.Delete")
		}

		namespace Dialogue
		{
			UE_DEFINE_GAMEPLAY_TAG(Open, "NU.SFX.Dialogue.Open")
			UE_DEFINE_GAMEPLAY_TAG(Advance, "NU.SFX.Dialogue.Advance")
			UE_DEFINE_GAMEPLAY_TAG(Close, "NU.SFX.Dialogue.Close")
		}

		namespace Quest
		{
			UE_DEFINE_GAMEPLAY_TAG(Offer, "NU.SFX.Quest.Offer")
			UE_DEFINE_GAMEPLAY_TAG(Accept, "NU.SFX.Quest.Accept")
			UE_DEFINE_GAMEPLAY_TAG(Progress, "NU.SFX.Quest.Progress")
			UE_DEFINE_GAMEPLAY_TAG(Ready, "NU.SFX.Quest.Ready")
			UE_DEFINE_GAMEPLAY_TAG(Complete, "NU.SFX.Quest.Complete")
			UE_DEFINE_GAMEPLAY_TAG(Reward, "NU.SFX.Quest.Reward")
		}

		namespace Field
		{
			UE_DEFINE_GAMEPLAY_TAG(Footstep, "NU.SFX.Field.Footstep")
			UE_DEFINE_GAMEPLAY_TAG(FootstepMale, "NU.SFX.Field.FootstepMale")
			UE_DEFINE_GAMEPLAY_TAG(FootstepArmor, "NU.SFX.Field.FootstepArmor")
			UE_DEFINE_GAMEPLAY_TAG(Interact, "NU.SFX.Field.Interact")
			UE_DEFINE_GAMEPLAY_TAG(PortalDepart, "NU.SFX.Field.PortalDepart")
			UE_DEFINE_GAMEPLAY_TAG(PortalArrive, "NU.SFX.Field.PortalArrive")
			UE_DEFINE_GAMEPLAY_TAG(DoorOpen, "NU.SFX.Field.DoorOpen")
			UE_DEFINE_GAMEPLAY_TAG(DoorClose, "NU.SFX.Field.DoorClose")
			UE_DEFINE_GAMEPLAY_TAG(Encounter, "NU.SFX.Field.Encounter")
			UE_DEFINE_GAMEPLAY_TAG(Heal, "NU.SFX.Field.Heal")
			UE_DEFINE_GAMEPLAY_TAG(Rest, "NU.SFX.Field.Rest")
			UE_DEFINE_GAMEPLAY_TAG(PointRegister, "NU.SFX.Field.PointRegister")
			UE_DEFINE_GAMEPLAY_TAG(AreaEnter, "NU.SFX.Field.AreaEnter")
			UE_DEFINE_GAMEPLAY_TAG(LevelUp, "NU.SFX.Field.LevelUp")
			UE_DEFINE_GAMEPLAY_TAG(JobChange, "NU.SFX.Field.JobChange")
			UE_DEFINE_GAMEPLAY_TAG(NaiaSummon, "NU.SFX.Field.NaiaSummon")
			UE_DEFINE_GAMEPLAY_TAG(NaiaUnsummon, "NU.SFX.Field.NaiaUnsummon")
			UE_DEFINE_GAMEPLAY_TAG(NaiaContract, "NU.SFX.Field.NaiaContract")
			UE_DEFINE_GAMEPLAY_TAG(Fall, "NU.SFX.Field.Fall")
			UE_DEFINE_GAMEPLAY_TAG(Wake, "NU.SFX.Field.Wake")
		}

		namespace Battle
		{
			UE_DEFINE_GAMEPLAY_TAG(Heal, "NU.SFX.Battle.Heal")
			UE_DEFINE_GAMEPLAY_TAG(LevelUp, "NU.SFX.Battle.LevelUp")
			UE_DEFINE_GAMEPLAY_TAG(NaiaSwap, "NU.SFX.Battle.NaiaSwap")
			UE_DEFINE_GAMEPLAY_TAG(CaptureSuccess, "NU.SFX.Battle.CaptureSuccess")
			UE_DEFINE_GAMEPLAY_TAG(Defeat, "NU.SFX.Battle.Defeat")
			UE_DEFINE_GAMEPLAY_TAG(Ready, "NU.SFX.Battle.Ready")
			UE_DEFINE_GAMEPLAY_TAG(TurnStart, "NU.SFX.Battle.TurnStart")
			UE_DEFINE_GAMEPLAY_TAG(Commit, "NU.SFX.Battle.Commit")
			UE_DEFINE_GAMEPLAY_TAG(CastBuff, "NU.SFX.Battle.CastBuff")
			UE_DEFINE_GAMEPLAY_TAG(Hit, "NU.SFX.Battle.Hit")
			UE_DEFINE_GAMEPLAY_TAG(Critical, "NU.SFX.Battle.Critical")
			UE_DEFINE_GAMEPLAY_TAG(Weak, "NU.SFX.Battle.Weak")
			UE_DEFINE_GAMEPLAY_TAG(Resist, "NU.SFX.Battle.Resist")
			UE_DEFINE_GAMEPLAY_TAG(ParrySuccess, "NU.SFX.Battle.ParrySuccess")
			UE_DEFINE_GAMEPLAY_TAG(DodgeSuccess, "NU.SFX.Battle.DodgeSuccess")
			UE_DEFINE_GAMEPLAY_TAG(ParryFail, "NU.SFX.Battle.ParryFail")
			UE_DEFINE_GAMEPLAY_TAG(DodgeFail, "NU.SFX.Battle.DodgeFail")
			UE_DEFINE_GAMEPLAY_TAG(ActionFail, "NU.SFX.Battle.ActionFail")
			UE_DEFINE_GAMEPLAY_TAG(CaptureFail, "NU.SFX.Battle.CaptureFail")
			UE_DEFINE_GAMEPLAY_TAG(SPRestore, "NU.SFX.Battle.SPRestore")
			UE_DEFINE_GAMEPLAY_TAG(Status, "NU.SFX.Battle.Status")
			UE_DEFINE_GAMEPLAY_TAG(DeathEnemy, "NU.SFX.Battle.DeathEnemy")
			UE_DEFINE_GAMEPLAY_TAG(DeathAlly, "NU.SFX.Battle.DeathAlly")
			UE_DEFINE_GAMEPLAY_TAG(CaptureThrow, "NU.SFX.Battle.CaptureThrow")
			UE_DEFINE_GAMEPLAY_TAG(CaptureShake, "NU.SFX.Battle.CaptureShake")
			UE_DEFINE_GAMEPLAY_TAG(Flee, "NU.SFX.Battle.Flee")
			UE_DEFINE_GAMEPLAY_TAG(Victory, "NU.SFX.Battle.Victory")
			UE_DEFINE_GAMEPLAY_TAG(Exit, "NU.SFX.Battle.Exit")

			namespace Windup
			{
				UE_DEFINE_GAMEPLAY_TAG(Sword, "NU.SFX.Battle.Windup.Sword")
				UE_DEFINE_GAMEPLAY_TAG(Bow, "NU.SFX.Battle.Windup.Bow")
				UE_DEFINE_GAMEPLAY_TAG(Magic, "NU.SFX.Battle.Windup.Magic")
				UE_DEFINE_GAMEPLAY_TAG(Card, "NU.SFX.Battle.Windup.Card")
				UE_DEFINE_GAMEPLAY_TAG(Beast, "NU.SFX.Battle.Windup.Beast")
			}

			namespace Attack
			{
				UE_DEFINE_GAMEPLAY_TAG(Punch, "NU.SFX.Battle.Attack.Punch")
				UE_DEFINE_GAMEPLAY_TAG(Slash, "NU.SFX.Battle.Attack.Slash")
				UE_DEFINE_GAMEPLAY_TAG(HeavySlash, "NU.SFX.Battle.Attack.HeavySlash")
				UE_DEFINE_GAMEPLAY_TAG(Arrow, "NU.SFX.Battle.Attack.Arrow")
				UE_DEFINE_GAMEPLAY_TAG(ArrowFinale, "NU.SFX.Battle.Attack.ArrowFinale")
				UE_DEFINE_GAMEPLAY_TAG(Card, "NU.SFX.Battle.Attack.Card")
				UE_DEFINE_GAMEPLAY_TAG(Orb, "NU.SFX.Battle.Attack.Orb")
				UE_DEFINE_GAMEPLAY_TAG(Magic, "NU.SFX.Battle.Attack.Magic")
				UE_DEFINE_GAMEPLAY_TAG(Heavy, "NU.SFX.Battle.Attack.Heavy")
				UE_DEFINE_GAMEPLAY_TAG(Beast, "NU.SFX.Battle.Attack.Beast")
				UE_DEFINE_GAMEPLAY_TAG(Roar, "NU.SFX.Battle.Attack.Roar")
				UE_DEFINE_GAMEPLAY_TAG(Wheel, "NU.SFX.Battle.Attack.Wheel")
				UE_DEFINE_GAMEPLAY_TAG(Ascend, "NU.SFX.Battle.Attack.Ascend")
				UE_DEFINE_GAMEPLAY_TAG(Lunge, "NU.SFX.Battle.Attack.Lunge")
				UE_DEFINE_GAMEPLAY_TAG(Fire, "NU.SFX.Battle.Attack.Fire")
				UE_DEFINE_GAMEPLAY_TAG(Water, "NU.SFX.Battle.Attack.Water")
				UE_DEFINE_GAMEPLAY_TAG(Grass, "NU.SFX.Battle.Attack.Grass")
				UE_DEFINE_GAMEPLAY_TAG(Earth, "NU.SFX.Battle.Attack.Earth")
				UE_DEFINE_GAMEPLAY_TAG(Lightning, "NU.SFX.Battle.Attack.Lightning")
				UE_DEFINE_GAMEPLAY_TAG(Wind, "NU.SFX.Battle.Attack.Wind")
			}

			namespace Buff
			{
				UE_DEFINE_GAMEPLAY_TAG(Guard, "NU.SFX.Battle.Buff.Guard")
				UE_DEFINE_GAMEPLAY_TAG(Mend, "NU.SFX.Battle.Buff.Mend")
				UE_DEFINE_GAMEPLAY_TAG(Focus, "NU.SFX.Battle.Buff.Focus")
				UE_DEFINE_GAMEPLAY_TAG(Ward, "NU.SFX.Battle.Buff.Ward")
				UE_DEFINE_GAMEPLAY_TAG(Haste, "NU.SFX.Battle.Buff.Haste")
			}

			namespace Impact
			{
				UE_DEFINE_GAMEPLAY_TAG(Punch, "NU.SFX.Battle.Impact.Punch")
				UE_DEFINE_GAMEPLAY_TAG(Slash, "NU.SFX.Battle.Impact.Slash")
				UE_DEFINE_GAMEPLAY_TAG(HeavyCut, "NU.SFX.Battle.Impact.HeavyCut")
				UE_DEFINE_GAMEPLAY_TAG(Pierce, "NU.SFX.Battle.Impact.Pierce")
				UE_DEFINE_GAMEPLAY_TAG(Card, "NU.SFX.Battle.Impact.Card")
				UE_DEFINE_GAMEPLAY_TAG(Claw, "NU.SFX.Battle.Impact.Claw")
				UE_DEFINE_GAMEPLAY_TAG(Bite, "NU.SFX.Battle.Impact.Bite")
				UE_DEFINE_GAMEPLAY_TAG(Magic, "NU.SFX.Battle.Impact.Magic")
				UE_DEFINE_GAMEPLAY_TAG(Heavy, "NU.SFX.Battle.Impact.Heavy")
				UE_DEFINE_GAMEPLAY_TAG(Shock, "NU.SFX.Battle.Impact.Shock")
				UE_DEFINE_GAMEPLAY_TAG(Fire, "NU.SFX.Battle.Impact.Fire")
				UE_DEFINE_GAMEPLAY_TAG(Water, "NU.SFX.Battle.Impact.Water")
				UE_DEFINE_GAMEPLAY_TAG(Grass, "NU.SFX.Battle.Impact.Grass")
				UE_DEFINE_GAMEPLAY_TAG(Earth, "NU.SFX.Battle.Impact.Earth")
				UE_DEFINE_GAMEPLAY_TAG(Lightning, "NU.SFX.Battle.Impact.Lightning")
				UE_DEFINE_GAMEPLAY_TAG(Wind, "NU.SFX.Battle.Impact.Wind")

				namespace Finale
				{
					UE_DEFINE_GAMEPLAY_TAG(Knight, "NU.SFX.Battle.Impact.Finale.Knight")
					UE_DEFINE_GAMEPLAY_TAG(Tracker, "NU.SFX.Battle.Impact.Finale.Tracker")
					UE_DEFINE_GAMEPLAY_TAG(Elementalist, "NU.SFX.Battle.Impact.Finale.Elementalist")
					UE_DEFINE_GAMEPLAY_TAG(Gambler, "NU.SFX.Battle.Impact.Finale.Gambler")
					UE_DEFINE_GAMEPLAY_TAG(Druid, "NU.SFX.Battle.Impact.Finale.Druid")
				}
			}

			namespace Strike
			{
				UE_DEFINE_GAMEPLAY_TAG(Body, "NU.SFX.Battle.Strike.Body")
				UE_DEFINE_GAMEPLAY_TAG(Fire, "NU.SFX.Battle.Strike.Fire")
				UE_DEFINE_GAMEPLAY_TAG(Water, "NU.SFX.Battle.Strike.Water")
				UE_DEFINE_GAMEPLAY_TAG(Grass, "NU.SFX.Battle.Strike.Grass")
				UE_DEFINE_GAMEPLAY_TAG(Earth, "NU.SFX.Battle.Strike.Earth")
				UE_DEFINE_GAMEPLAY_TAG(Lightning, "NU.SFX.Battle.Strike.Lightning")
				UE_DEFINE_GAMEPLAY_TAG(Wind, "NU.SFX.Battle.Strike.Wind")
			}

			namespace Boss
			{
				UE_DEFINE_GAMEPLAY_TAG(KnotGlow, "NU.SFX.Battle.Boss.KnotGlow")
				UE_DEFINE_GAMEPLAY_TAG(Stomp, "NU.SFX.Battle.Boss.Stomp")
				UE_DEFINE_GAMEPLAY_TAG(RootLash, "NU.SFX.Battle.Boss.RootLash")
				UE_DEFINE_GAMEPLAY_TAG(HillCollapse, "NU.SFX.Battle.Boss.HillCollapse")
			}
		}
	}
}
