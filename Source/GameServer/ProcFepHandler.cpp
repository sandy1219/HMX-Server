#include "ProcFepHandler.h"
#include "SceneUser.h"
#include "SceneUserMgr.h"
#include "GameService.h"
#include "CPlayer.hpp"
#include "CWorld.hpp"
#include "CRoom.hpp"
#include "CRoomMgr.hpp"
#include "CMaJiang.hpp"
#include "CMailMgr.hpp"
#include "CommFun.h"

struct stOtherRoleInfo
{
	std::string nick;
	std::string actor_addr;
};


ProcFepHandler::ProcFepHandler(void)
{
}

ProcFepHandler::~ProcFepHandler(void)
{
}

void ProcFepHandler::HandlePlayerExit(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}
	pPlayer->m_usLoginTime = time(NULL);
	CWorld::Instance()->LeaveGame(pMsg->clientSessID);
}

void ProcFepHandler::HnadleGotoScene(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	::msg_maj::ChangeSceneReq proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);

	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	if (pPlayer->GetRoomID() > 0)
	{
		return;
	}

	::msg_maj::TransterToSceneReq sendReq;
	sendReq.set_from_id(GameService::Instance()->GetServerID());
	sendReq.set_to_id(proto.scene_id());
	pPlayer->SetRoleProto(sendReq.add_role_list());
	GameService::Instance()->SendToWs(::comdef::msg_ss,::msg_maj::TransterToSceneReqID, sendReq);
}

void ProcFepHandler::HandleRoleReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	::msg_maj::PlayerInfoReq proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);

	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	CPlayer* lookPlayer = CWorld::Instance()->GetPlayerByUUID(proto.uid());
	if (NULL == lookPlayer)
	{
		return;
	}

	pPlayer->SendRoleData(lookPlayer);

}

void ProcFepHandler::HandleGpsUploadReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	::msg_maj::GpsUploadReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		return;
	}

	stGpsInfo gpsInfo;
	gpsInfo.longitude = proReq.longitude();
	gpsInfo.latitude = proReq.latitude();
	gpsInfo.address = proReq.address();
	pPlayer->SetGpsInfo(gpsInfo);
}

void ProcFepHandler::HandleUpdateReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	::msg_maj::UpdateRoleReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		return;
	}

	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}
	pPlayer->SendRoomCards(NULL);
}

void ProcFepHandler::HandleCardInfoReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	::msg_maj::CardinfoReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		return;
	}

	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	pPlayer->m_strReadname = proReq.readname();
	pPlayer->m_strReadcard = proReq.readcard();
	pPlayer->SaveDataToDB(0);
	
}

void ProcFepHandler::HandleRoomListReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	::msg_maj::RoomListReq  proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandleRoomListReq()";
		return;
	}

	//DUMP_PROTO_MSG(proReq);

	::msg_maj::RoomListResp proRep;
}

void ProcFepHandler::HandleOpenRoomReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	::msg_maj::OpenRoomReq  proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		return;
	}

	::msg_maj::OpenRoomResp proRep;
	if (pPlayer->GetRoomID() > 0)
	{
		CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
		if (pRoom) pRoom->SetToProto(proRep.mutable_info());
		proRep.set_code(::msg_maj::OpenRoomResp::ALREADY_IN);
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::open_room_resp, proRep);
		return;
	}

	// 是否维护时间
	int32_t today6 = GetTodayZero() + 6 * 3600;
	int32_t today8 = GetTodayZero() + 8 * 3600;
	int32_t nowTime = GetNowSecond();
	if (nowTime >= today6 && nowTime <= today8)
	{
		proRep.set_code(::msg_maj::OpenRoomResp::MAINTENANCE);
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::open_room_resp, proRep);
		return;
	}

	// 封停中
	if (pPlayer->IsBlocking())
	{
		proRep.set_code(::msg_maj::OpenRoomResp::FAIL);
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::open_room_resp, proRep);
		return;
	}

	// 检查需要的房卡
	int32_t usJuShuType = proReq.option().jushutype();
	int32_t usRenShuType = proReq.option().renshutype();
	uint16_t playNum = usRenShuType == 1 ? 4 : usRenShuType == 2 ? 3 : 2;
	int32_t nNeedRoodCards = (proReq.option().paytype() == 1 ? usJuShuType : usJuShuType * playNum);
	if (nNeedRoodCards < 1 || pPlayer->GetRoomCard() < nNeedRoodCards)
	{
		proRep.set_code(::msg_maj::OpenRoomResp::ROOMCARD_NOTENOUTH);
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::open_room_resp, proRep);
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->CreateRoom(proReq.room_type(), proReq.game_type(),pPlayer->GetCharID(), 0, proReq.option());
	if (NULL == pRoom)
	{
		proRep.set_code(::msg_maj::OpenRoomResp::FAIL);
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::open_room_resp, proRep);
		return;
	}

	pRoom->EnterPlayer(pPlayer);
	proRep.set_code(::msg_maj::OpenRoomResp::SUCCESS);
	pRoom->SetToProto(proRep.mutable_info());
	pPlayer->SetRoleInfoProto(proRep.mutable_role());
	pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::open_room_resp, proRep);
}

void ProcFepHandler::HandleEnterRoomReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	::msg_maj::EnterRoomReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandleEnterRoomReq()";
		return;
	}

	::msg_maj::EnterRoomResp proRep;
	if (pPlayer->GetRoomID() > 0)
	{
		CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
		if (pRoom) pRoom->SetToProto(proRep.mutable_info());
		proRep.set_code(msg_maj::EnterRoomResp::ALREADY_IN);
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::enter_room_resp, proRep);
		return;
	}

	// 封停中
	if (pPlayer->IsBlocking())
	{
		proRep.set_code(msg_maj::EnterRoomResp::FAIL);
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::enter_room_resp, proRep);
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(proReq.room_no());
	if (NULL == pRoom || pRoom->IsClose())
	{
		uint32_t unServerID = CWorld::Instance()->GetServerIDByRoomID(proReq.room_no());
		if (unServerID > 0 && unServerID != GameService::Instance()->GetServerID())
		{
			CWorld::Instance()->LeaveGame(pPlayer->GetSessionID());
			return;
		}

		proRep.set_code(msg_maj::EnterRoomResp::FAIL);
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::enter_room_resp, proRep);
		return;
	}

	if (pRoom->GetCurPersons() == pRoom->GetTotalPersons())
	{
		proRep.set_code(msg_maj::EnterRoomResp::PERSONS_FULL);
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::enter_room_resp, proRep);
		return;
	}

	// 检查需要的房卡
	int32_t nNeedRoodCards = pRoom->GetRule()->GetCostCard();
	if (nNeedRoodCards < 1 || pPlayer->GetRoomCard() < nNeedRoodCards)
	{
		::msg_maj::OpenRoomResp proRep2;
		proRep2.set_code(::msg_maj::OpenRoomResp::ROOMCARD_NOTENOUTH);
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::open_room_resp, proRep2);
		return;
	}

	if (pRoom->EnterPlayer(pPlayer))
	{
		proRep.set_code(msg_maj::EnterRoomResp::SUCCESS);
		proRep.set_self_seat(pPlayer->GetSeat());
		proRep.set_banker_seat(pRoom->GetBankerSeat());
		pRoom->SetToProto(proRep.mutable_info());
		pPlayer->SetRoleInfoProto(proRep.add_rolelist());
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::enter_room_resp, proRep);
	}
	else
	{
		proRep.set_code(msg_maj::EnterRoomResp::FAIL);
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::enter_room_resp, proRep);
	}
}

void ProcFepHandler::HandleLeaveRoomReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	::msg_maj::LeaveRoomReq  proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandleLeaveRoomReq()";
		return;
	}

	if (pPlayer->GetRoomID() <= 0)
	{
		::msg_maj::LeaveRoomResp proData;
		proData.set_code(::msg_maj::LeaveRoomResp::HAS_LEAVE_ROOM);
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::leave_room_resp, proData);
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom)
	{
		pRoom->LeavePlayer(pPlayer);
	}
}

void ProcFepHandler::HandleDismissRoomReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer || pPlayer->GetRoomID() <= 0)
	{
		return;
	}

	::msg_maj::DismissRoomReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandleDismissRoomReq()";
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom)
	{
		pRoom->DissmissRoomReq(pPlayer);
	}
}

void ProcFepHandler::HandleDismissAcceptReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		////LOG(ERROR) << "CMsgRoom::HandleDismissAcceptReq() NULL == pPlayer";
		return;
	}

	if (pPlayer->GetRoomID() <= 0)
	{
		////LOG(ERROR) << "CMsgRoom::HandleDismissAcceptReq(): " << pPlayer->GetRoomID();
		return;
	}

	::msg_maj::AgreeDismissReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandleReconnectReq()";
		return;
	}

	//DUMP_PROTO_MSG(proReq);

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom)
	{
		pRoom->DissmissAcceptReq(pPlayer, proReq.isagree());
	}
}

void ProcFepHandler::HandleKickRoleReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer || pPlayer->GetRoomID() <= 0)
	{
		return;
	}

	::msg_maj::KickRoleReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandleKickRoleReq()";
		return;
	}

	//DUMP_PROTO_MSG(proReq);

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom)
	{
		CPlayer* pBeKick = pRoom->GetPlayer(proReq.victim_seat());
		if (NULL == pBeKick)
		{
			return;
		}

		pRoom->KickRoleReq(pPlayer, proReq.victim_seat());

		if (pBeKick && pBeKick->IsDisconnect())
		{
			CWorld::Instance()->LeaveGame(pBeKick->GetSessionID());
		}
	}
}

void ProcFepHandler::HandleEnterRoomReady(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	if (pPlayer->GetRoomID() <= 0)
	{
		::msg_maj::NotifyRoomDismiss proData;
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::notify_room_dismiss, proData);
		return;
	}

	::msg_maj::EnterRoomReady proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandleEnterRoomReady()";
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom)
	{
		pRoom->EnterRoomReadyReq(pPlayer);
	}
}

void ProcFepHandler::HandleRoomCardPriceReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	::msg_maj::RoomCardPriceReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		return;
	}

	::msg_maj::RoomCardPriceResp proRep;
	proRep.add_room_cards(4);
	proRep.add_room_cards(8);
	proRep.add_room_cards(12);
	pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::roomcard_price_resp, proRep);
}

void ProcFepHandler::HandleStartGameReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer || pPlayer->GetRoomID() <= 0 || pPlayer->GetSeat() != 0)
	{
		return;
	}

	::msg_maj::StartGameReq  proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandleRoomListReq()";
		return;
	}

	//DUMP_PROTO_MSG(proReq);

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	pRoom->StartGameReq(pPlayer);
}

void ProcFepHandler::HandleAcceptStartReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer || pPlayer->GetRoomID() <= 0)
	{
		return;
	}

	::msg_maj::AcceptStartReq  proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandleAcceptStartReq()";
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom)
	{
		pRoom->AcceptAskReq(pPlayer, proReq.accept());
	}
}

void ProcFepHandler::HandleDisoverCardReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer || pPlayer->GetRoomID() <= 0)
	{
		return;
	}

	::msg_maj::DisCardOver  proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandleDisoverCardReq()";
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom)
	{
		pRoom->DisoverCardReq(pPlayer);
	}
}

void ProcFepHandler::HandleDiscardTileReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer || pPlayer->GetRoomID() <= 0)
	{
		return;
	}

	::msg_maj::DiscardReq  proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandleDisoverCardReq()";
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom)
	{
		if (pRoom->GetMajiang()->IsHasEvent())
		{
			////LOG(WARNING) << "has event not discard";
			return;
		}

		pRoom->DiscardTileReq(pPlayer, proReq.tile());
	}
}

void ProcFepHandler::HandlePrepareRoundReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer || pPlayer->GetRoomID() <= 0)
	{
		return;
	}

	::msg_maj::PrepareRoundReq  proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandlePrepareRoundReq()";
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom)
	{
		pRoom->PrepareRoundReq(pPlayer);
	}
}

void ProcFepHandler::HandleResponseReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer || pPlayer->GetRoomID() <= 0)
	{
		return;
	}

	::msg_maj::ResponseEventReq  proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandleResponseReq()";
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom)
	{
		pRoom->ResponseEventReq(pPlayer, proReq.event(), proReq.tile());
	}
}

void ProcFepHandler::HandleReconnectReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	::msg_maj::ReconnectReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		return;
	}

	//DUMP_PROTO_MSG(proReq);

	::msg_maj::ReconnectResp proRep;
}

void ProcFepHandler::HandleReconnectReadyReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	if (pPlayer->GetRoomID() <= 0)
	{
		::msg_maj::NotifyRoomDismiss proData;
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::notify_room_dismiss, proData);
		return;
	}

	::msg_maj::ReconnectReadyReq  proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandleResponseReq()";
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom)
	{
		pRoom->ReconnectReadyReq(pPlayer);
	}
}

void ProcFepHandler::HandleReconnectOtherReadyReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	if (pPlayer->GetRoomID() <= 0)
	{
		::msg_maj::NotifyRoomDismiss proData;
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::notify_room_dismiss, proData);
		return;
	}

	::msg_maj::ReconnectOtherReadyReq  proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandleResponseReq()";
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom)
	{
		pRoom->ReconnectOtherReadyReq(pPlayer);
	}
}

void ProcFepHandler::HandlerQuickMessageReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer || pPlayer->GetRoomID() <= 0)
	{
		return;
	}

	::msg_maj::QuickMessageReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (NULL == pRoom)
	{
		return;
	}

	::msg_maj::QuickMessageResp proSend;
	proSend.set_msg_index(proReq.msg_index());
	proSend.set_seat(proReq.seat());
	proSend.set_ret(0);

	pRoom->BrocastMsg(::comdef::msg_maj, ::msg_maj::quickmessage_resp, proSend, pPlayer);
}

void ProcFepHandler::HandlerQuickSanZhangReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer || pPlayer->GetRoomID() <= 0)
	{
		return;
	}

	::msg_maj::HuanSanZhangReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		//LOG(ERROR) << "HandlerHuanSanZhangReq()";
		return;
	}

	if (proReq.pais_size() < 3)
	{
		return;
	}

	std::vector<uint16_t> vecPai;
	for (int i = 0; i < proReq.pais_size(); ++i)
	{
		vecPai.push_back(proReq.pais(i));
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom)
	{
		pRoom->SanZhangReq(pPlayer, vecPai);
	}
}

void ProcFepHandler::HandlerQuickDingQueReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer || pPlayer->GetRoomID() <= 0)
	{
		return;
	}

	::msg_maj::DingQueReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		//LOG(ERROR) << "HandlerDingQueReq()";
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom)
	{
		pRoom->DingQuiReq(pPlayer, proReq.type());
	}
}

void ProcFepHandler::HandlerQuickMyScoreListReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer || pPlayer->GetRoomID() <= 0)
	{
		return;
	}

	::msg_maj::MyScoreListReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		//LOG(ERROR) << "HandlerDingQueReq()";
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom)
	{
		::msg_maj::MyScoreListResp resp;
		resp.set_myscore(pPlayer->GetTotalFan());
		resp.set_game_type(pRoom->GetMajType());
		for (std::vector<stScoreDetail>::const_iterator it = pPlayer->m_vecScoreDetail.begin(); it != pPlayer->m_vecScoreDetail.end(); ++it)
		{
			pRoom->FillHuTimesToDetail(*it, resp.add_scorelist());
		}
		pPlayer->SendMsgToClient(::comdef::msg_maj, ::msg_maj::my_scorelist_resp, resp);
	}
}

void ProcFepHandler::HandlerQuickMyTingListReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer || pPlayer->GetRoomID() <= 0)
	{
		return;
	}

	::msg_maj::MyTingPaiListReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		//LOG(ERROR) << "HandlerDingQueReq()";
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom)
	{
		bool bTurnMe = pRoom->GetMajiang()->m_eActionType == eActionType_SendCard && pRoom->GetMajiang()->m_usCurActionPos == pPlayer->GetSeat();
		pRoom->TingPaiWaitePai(pPlayer, bTurnMe);
	}
}

void ProcFepHandler::HandleRankTopList(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	msg_maj::RankReq proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);

	fogs::proto::msg::QueryRankReqResp sendProto;
	sendProto.set_ranktype(proto.type());
	sendProto.set_char_id(pPlayer->GetCharID());
	sendProto.set_last(proto.last());
	GameService::getMe().SendToWs(::comdef::msg_ss, ::msg_maj::QueryRankRequestID, sendProto);

}

void ProcFepHandler::HandleRemainingCardReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer || pPlayer->GetRoomID() <= 0)
	{
		return;
	}

	::msg_maj::RemainingCardsReq  proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandleRemainingCardReq()";
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom && pRoom->GetRoomStatus() == eRoomStatus_StartGame)
	{
		::msg_maj::RemainingCardsResp proResp;
		proResp.set_ret(1);

		std::vector<uint16_t> pailist = pRoom->GetMajiang()->GetRemainPaiList();
		for (std::vector<uint16_t>::iterator iter = pailist.begin(); iter != pailist.end(); ++iter)
		{
			proResp.add_cards(*iter);
		}

		pPlayer->SendMsgToClient(::comdef::msg_gm, ::msg_maj::remaining_card_resp, proResp);
	}
	else
	{
		::msg_maj::RemainingCardsResp proResp;
		proResp.set_ret(0);
		pPlayer->SendMsgToClient(::comdef::msg_gm, ::msg_maj::remaining_card_resp, proResp);
	}
}

void ProcFepHandler::HandleAssignAllCardsReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer || pPlayer->GetRoomID() <= 0)
	{
		return;
	}

	::msg_maj::AssignAllCardsReq  proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandleAssignAllCardsReq()";
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom && pRoom->GetRoomStatus() == eRoomStatus_Ready)
	{
		::msg_maj::AssignAllCardsResp proResp;
		proResp.set_ret(1);

		std::vector<uint16_t> pailist;
		//for (uint16_t i = 0; i < proReq.zjcards_size(); ++i)
		//{
		//	pailist.push_back(proReq.zjcards(i));
		//}
		//for (uint16_t i = 0; i < proReq.zxcards_size(); ++i)
		//{
		//	pailist.push_back(proReq.zxcards(i));
		//}
		//for (uint16_t i = 0; i < proReq.zdcards_size(); ++i)
		//{
		//	pailist.push_back(proReq.zdcards(i));
		//}
		//for (uint16_t i = 0; i < proReq.zscards_size(); ++i)
		//{
		//	pailist.push_back(proReq.zscards(i));
		//}
		//pailist.push_back(proReq.nextcard());

		pailist.push_back(11);
		pailist.push_back(11);
		pailist.push_back(11);
		pailist.push_back(12);
		pailist.push_back(12);
		pailist.push_back(12);
		pailist.push_back(13);
		pailist.push_back(13);
		pailist.push_back(13);
		pailist.push_back(14);
		pailist.push_back(14);
		pailist.push_back(37);
		pailist.push_back(39);

		pailist.push_back(21);
		pailist.push_back(21);
		pailist.push_back(21);
		pailist.push_back(22);
		pailist.push_back(22);
		pailist.push_back(22);
		pailist.push_back(23);
		pailist.push_back(23);
		pailist.push_back(23);
		pailist.push_back(24);
		pailist.push_back(24);
		pailist.push_back(37);
		pailist.push_back(39);

		pailist.push_back(31);
		pailist.push_back(31);
		pailist.push_back(31);
		pailist.push_back(32);
		pailist.push_back(32);
		pailist.push_back(32);
		pailist.push_back(33);
		pailist.push_back(33);
		pailist.push_back(33);
		pailist.push_back(34);
		pailist.push_back(34);
		pailist.push_back(37);
		pailist.push_back(39);

		pailist.push_back(35);
		pailist.push_back(35);
		pailist.push_back(35);
		pailist.push_back(36);
		pailist.push_back(36);
		pailist.push_back(36);
		pailist.push_back(31);
		pailist.push_back(32);
		pailist.push_back(33);
		pailist.push_back(34);
		pailist.push_back(34);
		pailist.push_back(37);
		pailist.push_back(39);

		pailist.push_back(38);

		pRoom->GetMajiang()->GMSetMajiang(pailist);

		pPlayer->SendMsgToClient(::comdef::msg_gm, ::msg_maj::assign_all_cards_resp, proResp);
	}
	else
	{
		::msg_maj::AssignAllCardsResp proResp;
		proResp.set_ret(0);
		pPlayer->SendMsgToClient(::comdef::msg_gm, ::msg_maj::assign_all_cards_resp, proResp);
	}
}

void ProcFepHandler::HandleAssignNextCardReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer || pPlayer->GetRoomID() <= 0)
	{
		return;
	}

	::msg_maj::AssignNextCardReq  proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		////LOG(ERROR) << "HandleAssignAllCardsReq()";
		return;
	}

	CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
	if (pRoom && pRoom->GetRoomStatus() == eRoomStatus_StartGame)
	{
		::msg_maj::AssignNextCardResp proResp;
		proResp.set_ret(1);

		pRoom->GetMajiang()->GMSetPai(proReq.card());

		pPlayer->SendMsgToClient(::comdef::msg_gm, ::msg_maj::assign_next_card_resp, proResp);
	}
	else
	{
		::msg_maj::AssignNextCardResp proResp;
		proResp.set_ret(0);
		pPlayer->SendMsgToClient(::comdef::msg_gm, ::msg_maj::assign_next_card_resp, proResp);
	}
}

void ProcFepHandler::HandleCommonReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	::msg_maj::GMCommonOperResp  proSend;
	proSend.set_ret(0);

	::msg_maj::GMCommonOperReq  proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		proSend.set_ret(1);
		pPlayer->SendMsgToClient(::comdef::msg_gm, ::msg_maj::gm_common_oper_resp, proSend);
		return;
	}

	std::vector<std::string> vecStr;
	stringtok(vecStr, proReq.content());

	if (!vecStr.empty())
	{
		if (strstr(vecStr[0].c_str(), "scoreadd"))
		{
			if (vecStr.size() == 2)
			{
				int addVal = atoi(vecStr[1].c_str());
				pPlayer->SetTopScoreTotal(pPlayer->GetTopScoreTotal() + addVal);
				pPlayer->OnChangeScore();
			}
		}
		else if (strstr(vecStr[0].c_str(), "winsadd"))
		{
			if (vecStr.size() == 2)
			{
				int addVal = atoi(vecStr[1].c_str());
				pPlayer->SetTopWinsTotal(pPlayer->GetTopWinsTotal() + addVal);
				pPlayer->OnChangeWins();
			}
		}
		else if (strstr(vecStr[0].c_str(), "mailadd"))
		{
			std::stringstream ssTitle;
			ssTitle << "Title-" << (time(NULL) / 10000);
			std::stringstream ssContent;
			ssContent << "Content-" << (time(NULL) / 10000);
			//CMailMgr::Instance()->SendToPlayer(*pPlayer, ssTitle.str(), ssContent.str(), randBetween(0, 5));
		}
		else if (strstr(vecStr[0].c_str(), "mailcommon"))
		{
			std::stringstream ssTitle;
			ssTitle << "Title-Common-" << (time(NULL) / 10000);
			std::stringstream ssContent;
			ssContent << "Content-Common-" << (time(NULL) / 10000);
			//CMailMgr::Instance()->SendToPlayer(0, "", 0, "", ssTitle.str(), ssContent.str(), 0);
		}
		else if (strstr(vecStr[0].c_str(), "notices")) // 重新刷新通告
		{

		}
		else if (strstr(vecStr[0].c_str(), "roomcardadd"))
		{
			if (vecStr.size() == 2)
			{
				int addVal = atoi(vecStr[1].c_str());
				if (addVal > 0)
					pPlayer->AddRoomCards(addVal, ::fogs::proto::msg::log_t_roomcard_add_gm);
				else
					pPlayer->SubRoomCards(-addVal, ::fogs::proto::msg::log_t_roomcard_sub_gm);
				pPlayer->SendRoomCards();
			}
		}
		else
		{
			proSend.set_ret(1);
		}
	}
	else
	{
		proSend.set_ret(1);
	}

	pPlayer->SendMsgToClient(::comdef::msg_gm, ::msg_maj::gm_common_oper_resp, proSend);

}



void ProcFepHandler::HandleListReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	::msg_maj::MailListReq proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (pPlayer == NULL)
		return;
	CMailMgr::Instance()->SendMailList(pPlayer, proto.start_index());
}

void ProcFepHandler::HandleOptReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{

	::msg_maj::MailOptReq proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);

	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	CMail* obj = NULL;
	if (!CMailMgr::Instance()->GetByID(obj, proto.id()))
	{
		return;
	}

	if (obj->GetToID() > 0)
	{
		if (obj->GetToID() != pPlayer->GetCharID())
		{
			return;
		}
		bool bResutl = false;
		if (proto.opt() == ::msg_maj::opt_t_read)
		{
			bResutl = obj->DoRead();
		}
		else if (proto.opt() == ::msg_maj::opt_t_fetch)
		{
			bResutl = obj->DoFetchAward();
		}
		else if (proto.opt() == ::msg_maj::opt_t_delete)
		{
			bResutl = obj->DoDelete();
		}
		else
		{
			return;
		}

		if (!bResutl)
		{
			return;
		}
	}
	else // 系统公共邮件
	{
		bool bResutl = false;
		CMailSysLog* objSysLog = CMailSysLogMgr::Instance()->GetByUIDMailID(pPlayer->GetCharID(), obj->GetMailID());
		if (objSysLog == NULL)
		{
			::msg_maj::MailSystemLogS record;
			zUUID zuuid;
			record.set_id(zuuid.generate());
			record.set_uid(pPlayer->GetCharID());
			record.set_mail_id(obj->GetMailID());
			record.set_mark(obj->GetMark());
			record.set_create_time(obj->GetCreateTime());
			record.set_fetch_time(0);
			objSysLog = CMailSysLogMgr::Instance()->AddRecord(record, true);
			if (objSysLog == NULL)
			{
				return;
			}
		}

		if (proto.opt() == ::msg_maj::opt_t_read)
		{
			bResutl = objSysLog->DoRead();
		}
		else if (proto.opt() == ::msg_maj::opt_t_fetch)
		{
			bResutl = objSysLog->DoFetchAward();
		}
		else if (proto.opt() == ::msg_maj::opt_t_delete)
		{
			bResutl = objSysLog->DoDelete();
		}
		else
		{
			return;
		}

		if (!bResutl)
		{
			return;
		}
	}
}

void ProcFepHandler::HandleHistoryReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	::msg_maj::HistoryListReq proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);

	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}
	::msg_maj::HistoryRoomResp sendProto;
	sendProto.set_type(proto.type());

	if (proto.type() == ::msg_maj::room_list)
	{
		if (pPlayer->GetRoomID() <= 0 || pPlayer->GetSeat() > 4)
		{
			return;
		}

		CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
		if (NULL == pRoom)
		{
			return;
		}

		::msg_maj::RoomInfo roomInfo;
		if (pRoom->GetRoomInfo(roomInfo))
		{
			::msg_maj::RoleInfoListS roleList;
			pRoom->GetRoleInfoList(roleList);
			::msg_maj::HistoryRecordS proto;
			proto.set_record_id(pRoom->GetRecordID());
			proto.set_room_id(pRoom->GetRoomID());
			proto.set_time(pRoom->GetZjStartTime());
			proto.mutable_room_info()->CopyFrom(roomInfo);
			proto.mutable_role_info()->CopyFrom(roleList);

			pRoom->GetInnRecordListS(proto.mutable_innrecord());

			CountTotalScore(proto);

			Recond2ProtoCRoom(proto, sendProto);
			//DUMP_PROTO_MSG(sendProto);
			pPlayer->SendMsgToClient(::comdef::msg_hist, ::msg_maj::history_list_resp, sendProto);
		}
		else
		{
			return;
		}

	}
	else if (proto.type() == ::msg_maj::total_list)
	{
		fogs::proto::msg::ZhanjiQueryList protoList;
		protoList.set_char_id(pPlayer->GetCharID());
		protoList.mutable_req()->CopyFrom(proto);
		GameService::Instance()->SendToDp(::comdef::msg_ss, ::msg_maj::ZhanjiQueryListRequestID,protoList);
	}
	else
	{
		return;
	}
}

void ProcFepHandler::HandleHistRoomReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	::msg_maj::HistoryRoomReq proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);

	::msg_maj::HistoryRoomResp sendProto;
	sendProto.set_type(proto.type());

	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	if (proto.type() == ::msg_maj::room_list)
	{
		if (pPlayer->GetRoomID() <= 0 || pPlayer->GetSeat() > 4)
		{
			return;
		}

		CRoom* pRoom = CRoomMgr::Instance()->GetRoomByID(pPlayer->GetRoomID());
		if (NULL == pRoom)
		{
			return;
		}

		::msg_maj::RoomInfo roomInfo;
		if (pRoom->GetRoomInfo(roomInfo))
		{
			::msg_maj::RoleInfoListS roleList;
			pRoom->GetRoleInfoList(roleList);
			::msg_maj::HistoryRecordS proto;
			proto.set_record_id(pRoom->GetRecordID());
			proto.set_room_id(pRoom->GetRoomID());
			proto.set_time(pRoom->GetZjStartTime());
			proto.mutable_room_info()->CopyFrom(roomInfo);
			proto.mutable_role_info()->CopyFrom(roleList);

			pRoom->GetInnRecordListS(proto.mutable_innrecord());
	
			CountTotalScore(proto);
			Recond2ProtoCRoom(proto, sendProto);

			pPlayer->SendMsgToClient(::comdef::msg_hist, ::msg_maj::history_room_resp, sendProto);
		}
		else
		{
			return;
		}

	}
	else if (proto.type() == ::msg_maj::total_list)
	{
		fogs::proto::msg::ZhanjiQueryRoom protoList;
		protoList.set_char_id(pPlayer->GetCharID());
		protoList.mutable_req()->CopyFrom(proto);
		GameService::Instance()->SendToDp(::comdef::msg_ss, ::msg_maj::ZhanjiQueryRoomRequestID, protoList);
	}
	else
	{
		return;
	}
}

void ProcFepHandler::HandleHistInnReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	::msg_maj::HistoryInnReq proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);

	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	fogs::proto::msg::ZhanjiQueryInnReq protoList;
	protoList.set_char_id(pPlayer->GetCharID());
	protoList.mutable_req()->CopyFrom(proto);
	GameService::Instance()->SendToDp(::comdef::msg_ss, ::msg_maj::ZhanjiQueryInnRequestID, protoList);
}

void ProcFepHandler::HandleReplayReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	::msg_maj::ReplayReq proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);

	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
		return;

	fogs::proto::msg::ZhanjiQueryReply sendProto;
	sendProto.set_char_id(pPlayer->GetCharID());
	sendProto.mutable_req()->CopyFrom(proto);

	GameService::Instance()->SendToDp(::comdef::msg_ss, ::msg_maj::ZhanjiQueryReplyRequestID, sendProto);

}

void ProcFepHandler::CountTotalScore(::msg_maj::HistoryRecordS& srcProtoData)
{
	// 计算总分 
	std::map<uint32_t, ::msg_maj::SeatTotalScoreS> mapSeatInfo;

	const ::msg_maj::InnRecordListS& innList = srcProtoData.innrecord();
	for (int i = 0; i < innList.inn_list_size(); ++i)
	{
		// 第几场
		const ::msg_maj::InnRecordS& innRow = innList.inn_list(i);

		for (int j = 0; j < innRow.seat_result_size(); ++j)
		{
			const ::msg_maj::GameResultSeat& seatInfo = innRow.seat_result(j);
			::msg_maj::SeatTotalScoreS& seatScore = mapSeatInfo[seatInfo.seat()];
			seatScore.set_seat_id(seatInfo.seat());
			seatScore.set_score(seatScore.score() + seatInfo.total_score());
		}
	}

	::msg_maj::SeatTotalS* seatTotalS = srcProtoData.mutable_seat_total();
	if (seatTotalS)
	{
		seatTotalS->clear_score_list();
		for (std::map<uint32_t, ::msg_maj::SeatTotalScoreS>::iterator it = mapSeatInfo.begin(); it != mapSeatInfo.end(); ++it)
		{
			seatTotalS->add_score_list()->CopyFrom(it->second);
		}
	}
}

void ProcFepHandler::Recond2ProtoC(const ::msg_maj::HistoryRecordS& srcProto, ::msg_maj::HistoryRecord& distProto)
{
	distProto.set_record_id(srcProto.record_id());
	distProto.set_room_id(srcProto.room_id());
	distProto.mutable_option()->CopyFrom(srcProto.room_info().option());
	distProto.set_time(srcProto.time());

	std::map<int32_t, stOtherRoleInfo> mapSeatName;
	const ::msg_maj::RoleInfoListS& roleInfos = srcProto.role_info();
	for (int j = 0; j < roleInfos.role_list_size(); ++j)
	{
		stOtherRoleInfo otherInfo;
		otherInfo.nick = roleInfos.role_list(j).nick();
		otherInfo.actor_addr = roleInfos.role_list(j).actor_addr();
		mapSeatName.insert(std::make_pair(roleInfos.role_list(j).seat(), otherInfo));
	}

	for (int i = 0; i < srcProto.seat_total().score_list_size(); ++i)
	{
		::msg_maj::SeatTotalScore* seatTotalInfo = distProto.add_score_list();
		if (seatTotalInfo)
		{
			const ::msg_maj::SeatTotalScoreS& seatTotalData = srcProto.seat_total().score_list(i);
			seatTotalInfo->set_seat_id(seatTotalData.seat_id());
			seatTotalInfo->set_score(seatTotalData.score());
			seatTotalInfo->set_nickname(mapSeatName[seatTotalData.seat_id()].nick);
			seatTotalInfo->set_actor_addr(mapSeatName[seatTotalData.seat_id()].actor_addr);
		}
	}
}

void ProcFepHandler::Recond2ProtoCRoom(const ::msg_maj::HistoryRecordS& srcProto, ::msg_maj::HistoryRoomResp& distProto)
{
	distProto.set_record_id(srcProto.record_id());
	distProto.set_room_id(srcProto.room_id());
	distProto.mutable_option()->CopyFrom(srcProto.room_info().option());
	for (int i = 0; i < srcProto.innrecord().inn_list_size(); ++i)
	{
		::msg_maj::InnRecord* innInfo = distProto.add_inn_list();
		if (innInfo)
		{
			const ::msg_maj::InnRecordS& innData = srcProto.innrecord().inn_list(i);
			innInfo->set_inn_id(innData.inn_id());
			innInfo->set_banker_seat(innData.banker_seat());
			innInfo->set_dice(innData.dice());
			innInfo->set_close_type(innData.close_type());
			for (int j = 0; j < innData.seat_result_size(); ++j)
			{
				innInfo->add_seat_result()->CopyFrom(innData.seat_result(j));
			}
		}
	}

	std::map<int32_t, stOtherRoleInfo> mapSeatName;
	const ::msg_maj::RoleInfoListS& roleInfos = srcProto.role_info();
	for (int j = 0; j < roleInfos.role_list_size(); ++j)
	{
		stOtherRoleInfo otherInfo;
		otherInfo.nick = roleInfos.role_list(j).nick();
		otherInfo.actor_addr = roleInfos.role_list(j).actor_addr();
		mapSeatName.insert(std::make_pair(roleInfos.role_list(j).seat(), otherInfo));
	}

	for (int i = 0; i < srcProto.seat_total().score_list_size(); ++i)
	{
		::msg_maj::SeatTotalScore* seatTotalInfo = distProto.add_score_list();
		if (seatTotalInfo)
		{
			const ::msg_maj::SeatTotalScoreS& seatTotalData = srcProto.seat_total().score_list(i);
			seatTotalInfo->set_seat_id(seatTotalData.seat_id());
			seatTotalInfo->set_score(seatTotalData.score());
			seatTotalInfo->set_nickname(mapSeatName[seatTotalData.seat_id()].nick);
			seatTotalInfo->set_actor_addr(mapSeatName[seatTotalData.seat_id()].actor_addr);
		}
	}
}

void ProcFepHandler::HandleMatchBaoMingReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{

}

void ProcFepHandler::HandleMatchOpenRoomReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{

}

void ProcFepHandler::HandleMatchSortRankReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize) 
{

}

void ProcFepHandler::doPlayerExit(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	H::logger->info("玩家离线");
}

void ProcFepHandler::doChatWorld(zSession* pSession, const PbMsgWebSS* pMsg,int32_t nSize)
{

}

void ProcFepHandler::doClientIsReady(zSession* pSession, const PbMsgWebSS* pMsg,int32_t nSize)
{

}

void ProcFepHandler::doQuestAccept(zSession* pSession, const PbMsgWebSS* pMsg,int32_t nSize)
{

}

void ProcFepHandler::doChangeScene(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	return;
}

void ProcFepHandler::doItemMovePosition(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	//const C::RqItemMovePosition* packet = static_cast<const C::RqItemMovePosition*>(pMsg);
	//SceneUser* pUser = GameService::getMe().getSceneUserMgr()->getUserBySessID(packet->clientSessID);
	//if (pUser == NULL)
	//{
	//	ASSERT(pUser);
	//	return;
	//}

}

void ProcFepHandler::HandleGetShareInfoReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	::msg_maj::GetShareInfoReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		return;
	}

	::msg_maj::GetShareInfoResp proResp;
	proResp.set_status(pPlayer->GetFirstShowStatus());
	proResp.set_room_card(CWorld::Instance()->GetShareRewardConfig().room_card());
	pPlayer->SendMsgToClient(::comdef::msg_activity, ::msg_maj::get_share_info_resp, proResp);

	//DUMP_PROTO_MSG_INFO(proResp);
}

void ProcFepHandler::HandleShareReportReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	::msg_maj::ShareReportReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		return;
	}

	if (pPlayer->GetFirstShowStatus() == 0)
	{
		pPlayer->SetFirstShowStatus(1);
	}

	::msg_maj::ShareReportResp proResp;
	proResp.set_share_status(pPlayer->GetFirstShowStatus());
	proResp.set_today_status1(pPlayer->m_usTodayPlayStatus1);
	proResp.set_today_status2(pPlayer->m_usTodayPlayStatus2);
	pPlayer->SendMsgToClient(::comdef::msg_activity, ::msg_maj::share_Report_resp, proResp);

}

void ProcFepHandler::HandleRecvShareRewardReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	::msg_maj::RecvShareRewardReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		return;
	}

	::msg_maj::RecvShareRewardResp proResp;
	if (pPlayer->GetFirstShowStatus() != 1)
	{
		proResp.set_code(::msg_maj::RecvShareRewardResp::FAIL);
	}
	else
	{
		proResp.set_code(::msg_maj::RecvShareRewardResp::SUCCESS);
		pPlayer->SetFirstShowStatus(2);

		pPlayer->AddRoomCards(CWorld::Instance()->GetShareRewardConfig().room_card(), ::fogs::proto::msg::log_t_roomcard_add_share);
		pPlayer->SendRoomCards();
	}
	proResp.set_status(pPlayer->GetFirstShowStatus());
	pPlayer->SendMsgToClient(::comdef::msg_activity, ::msg_maj::Recv_share_reward_resp, proResp);

}

void ProcFepHandler::HandleGetInvitationInfoReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	::msg_maj::GetInvitationInfoReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		return;
	}

	::msg_maj::GetInvitationInfoResp proResp;
	proResp.set_inv_friend_room_card(CWorld::Instance()->GetInvitationRewardConfig().room_card());
	proResp.set_has_inv_friend_num(pPlayer->GetHasInvFriendNum());
	proResp.set_has_get_room_card(pPlayer->GetHasGetRoomCard());
	proResp.set_can_get_room_card(pPlayer->GetCanGetRoomCard());

	proResp.set_invitation_player(pPlayer->GetBeInvitationName());
	if (8 >= pPlayer->GetPlayGames())
	{
		proResp.set_need_player_games(8 - pPlayer->GetPlayGames());
	}
	else
	{
		proResp.set_need_player_games(0);
	}

	proResp.set_reward_room_card(5);
	proResp.set_recv_reward_status(pPlayer->GetRecvRewardStatus());
	pPlayer->SendMsgToClient(::comdef::msg_activity, ::msg_maj::get_invitation_info_resp, proResp);

}

void ProcFepHandler::HandleRecvInvitationRewardReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	::msg_maj::RecvInvitationRewardReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		return;
	}

	::msg_maj::RecvInvitationRewardResp proResp;
	proResp.set_reward_type(proReq.reward_type());
	if (proReq.reward_type() == 1) //主动邀请奖励
	{
		if (pPlayer->GetCanGetRoomCard() == 0)
		{
			proResp.set_code(::msg_maj::RecvInvitationRewardResp::FAIL);
		}
		else
		{
			proResp.set_code(::msg_maj::RecvInvitationRewardResp::SUCCESS);

			pPlayer->SetHasGetRoomCard(pPlayer->GetHasGetRoomCard() + pPlayer->GetCanGetRoomCard());
			pPlayer->AddRoomCards(pPlayer->GetCanGetRoomCard(), ::fogs::proto::msg::log_t_roomcard_add_inv);
			pPlayer->SendRoomCards();
			pPlayer->SetCanGetRoomCard(0);

			proResp.set_has_get_room_card(pPlayer->GetHasGetRoomCard());
			proResp.set_can_get_room_card(pPlayer->GetCanGetRoomCard());
			proResp.set_recv_reward_status(pPlayer->GetRecvRewardStatus());
		}
	}
	else if (proReq.reward_type() == 2) //被邀请玩游戏后的奖励
	{
		if (pPlayer->GetRecvRewardStatus() != 1)
		{
			proResp.set_code(::msg_maj::RecvInvitationRewardResp::FAIL);
		}
		else
		{
			proResp.set_code(::msg_maj::RecvInvitationRewardResp::SUCCESS);

			pPlayer->SetRecvRewardStatus(2);

			proResp.set_has_get_room_card(pPlayer->GetHasGetRoomCard());
			proResp.set_can_get_room_card(pPlayer->GetCanGetRoomCard());
			proResp.set_recv_reward_status(pPlayer->GetRecvRewardStatus());
		}
	}

	pPlayer->SendMsgToClient(::comdef::msg_activity, ::msg_maj::Recv_invitation_reward_resp, proResp);
}

void ProcFepHandler::HandleGetBindingAgentInfoReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	::msg_maj::GetBindingAgentInfoReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		return;
	}

	fogs::proto::msg::GetBindingAgentInfoReq proData;
	proData.set_user_id(pPlayer->GetCharID());
	GameService::Instance()->SendToWs(::comdef::msg_ss,::msg_maj::GetBindingAgentInfoReqID,pMsg->clientSessID,pMsg->fepServerID,proData);
}

void ProcFepHandler::HandleBindingAgentReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	::msg_maj::BindingAgentReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		return;
	}

	fogs::proto::msg::BindingAgentReq proData;
	proData.set_user_id(pPlayer->GetCharID());
	proData.set_agent_id(proReq.user_id());
	GameService::Instance()->SendToWs(::comdef::msg_ss, ::msg_maj::BindingAgentReqID,pMsg->clientSessID,pMsg->fepServerID, proData);
}

void ProcFepHandler::HandleInputInvitationCodeReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	::msg_maj::InputInvitationCodeReq proReq;
	if (!proReq.ParseFromArray(pMsg->data, pMsg->size))
	{
		return;
	}

	fogs::proto::msg::InputInviteCodeReq pro;
	pro.set_user_id(pPlayer->GetCharID());
	pro.set_code(proReq.invitation_code());
	GameService::Instance()->SendToWs(::comdef::msg_ss, ::msg_maj::InputInviteCodeReqID, pMsg->clientSessID, pMsg->fepServerID, pro);
}

void ProcFepHandler::doPositionMove(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	//const C::RqPositionMove* packet = static_cast<const C::RqPositionMove*>(pMsg);
	//SceneUser* pUser = GameService::getMe().getSceneUserMgr()->getUserBySessID(packet->clientSessID);
	//if (pUser == NULL)
	//{
	//	ASSERT(pUser);
	//	return;
	//}

	////H::logger->info("SceneManager::loadScene type=%d,countryid=%d,mapid=%d", 1, 2, 3);

	//int32_t x = packet->nNewX;
	//int32_t y = packet->nNewY;

	//t_NpcAIDefine ai(NPC_AI_MOVETO,zPos(x, y),10,10,60);
	//pUser->AIC->setAI(ai);

}

void ProcFepHandler::doShoppingBuyItem(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{

	//const C::RqShoppingBuyItem* packet = static_cast<const C::RqShoppingBuyItem*>(pMsg);
	//SceneUser* pUser = GameService::getMe().getSceneUserMgr()->getUserBySessID(packet->clientSessID);
	//if (pUser == NULL)
	//{
	//	ASSERT(pUser);
	//	return;
	//}

	//int32_t count = pUser->ucm.GetCounter(Counter_ITEM_SHOP);

	//H::logger->debug("Counter:%u", count);

	//pUser->ucm.AddDay(Counter_ITEM_SHOP, 1);

	//zShopB* shopB = shopbm.get(packet->nShopID);
	//if (shopB == NULL)
	//{
	//	H::logger->error("Not found shopID:%u", packet->nShopID);
	//	return;
	//}

	//int32_t nItemNum = shopB->itemNum * packet->nShopNum;

	//if (!pUser->TryAddObject(shopB->itemID, nItemNum))
	//{
	//	H::logger->debug("try add item fail");
	//	return;
	//}

	//bool ret = pUser->SubMoney(shopB->sellMoneyType, shopB->sellMoneyValue);
	//if (!ret)
	//{
	//	H::logger->debug("money isnout enough");
	//	return;
	//}

	//// 增加背包道具 
	//ret = pUser->AddObject(shopB->itemID, nItemNum, true);

	//if (!ret)
	//{
	//	H::logger->debug("buy shop fail");
	//	return;
	//}

	//H::logger->debug("buy shop success");
}

void ProcFepHandler::doShoppingSellItem(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{

}

void ProcFepHandler::doUseItem(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{

}

void ProcFepHandler::doChanneCmd(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{

}

/* 单聊 */
void ProcFepHandler::doChatToOne(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	//const RqChatToOne* packet = static_cast<const RqChatToOne*>(pMsg);
	//SceneUser* pUser = NetService::getMe().getSceneUserMgr()->getUserBySessID(packet->clientSessID);
	//if (pUser == NULL)
	//{
	//	ASSERT(pUser);
	//	return;
	//}

	//pUser->ChatToOne(packet,nSize);

	/* */
}

/* 群聊 */
void ProcFepHandler::doChatToTeam(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{

}

/* 讨论组 */
void ProcFepHandler::doChatToDiscuss(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{

}

/* 世界聊 */
void ProcFepHandler::doChatToWorld(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{

	//const C::RqChatToWorld* packet = static_cast<const C::RqChatToWorld*>(pMsg);
	//SceneUser* pUser = GameService::getMe().getSceneUserMgr()->getUserBySessID(packet->clientSessID);
	//if (pUser == NULL)
	//{
	//	ASSERT(pUser);
	//	return;
	//}

	//H::logger->info("世界聊天:%s", zUtility::Utf8ToGBK(packet->msg.data));

	//pUser->SendToWs(pMsg,nSize);

}

void ProcFepHandler::doRelationList(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{


}

void ProcFepHandler::doRelationAdd(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	
}

void ProcFepHandler::doRelationRemove(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{

}


