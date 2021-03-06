#include "ProcSsHandler.h"
#include "SceneRegMgr.h"
#include "WorldUserMgr.h"
#include "GameService.h"
#include "CSortsManager.hpp"
#include "SceneRoom.h"
#include "SceneRoomMgr.h"
#include "MysqlProtobufHelper.h"
#include "curl.h"
#include "OfflineUserMgr.h"

#include "Poco/JSON/Parser.h"
#include "Poco/JSON/ParseHandler.h"
#include "Poco/JSON/JSONException.h"
#include "Poco/StreamCopier.h"
#include "Poco/Dynamic/Var.h"
#include "Poco/JSON/Query.h"
#include "Poco/JSON/PrintHandler.h"

using namespace Poco::Dynamic;
using namespace Poco;

ProcSsHandler::ProcSsHandler()
{
}

ProcSsHandler::~ProcSsHandler()
{
}

void ProcSsHandler::TransTerToSceneReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	::msg_maj::TransterToSceneReq proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);

	zSession* pScene = GameService::getMe().SessionMgr()->getSs(proto.to_id());
	if (pScene == NULL)
	{
		return;
	}
	pScene->sendMsgProto(::comdef::msg_ss, ::msg_maj::TransterToSceneReqID, proto);
}

void ProcSsHandler::TransTerToSceneResp(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{

}

void ProcSsHandler::NotifyUpdateRankInfo(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	fogs::proto::msg::UpdateSaveRanks proLists;
	proLists.ParseFromArray(pMsg->data, pMsg->size);

	uint64_t nCharID = 0;

	switch (proLists.ranktype())
	{
	case msg_maj::rank_t_wins:
	{
		SortWinsKey key;
		key.uid = proLists.winsinfo().uid();
		key.name = proLists.winsinfo().name();
		key.value = proLists.winsinfo().value();
		key.time = time(NULL);
		SortWinsValue value(key);
		value.actor_addr = proLists.winsinfo().actor_addr();
		CSortsManager::Instance()->Update(key, value);
		nCharID = key.uid;
	}
	break;
	case msg_maj::rank_t_score:
	{
		SortScoreKey key;
		key.uid = proLists.scoreinfo().uid();
		key.name = proLists.scoreinfo().name();
		key.value = proLists.scoreinfo().value();
		key.time = time(NULL);
		SortScoreValue value(key);
		value.actor_addr = proLists.scoreinfo().actor_addr();
		CSortsManager::Instance()->Update(key, value);
		nCharID = key.uid;
	}
	break;
	default:
		break;
	}

	std::pair<int32_t, int32_t> myRankScore;
	CSortsManager::Instance()->GetMyTopList(nCharID, proLists.ranktype(), myRankScore);

	fogs::proto::msg::UpdatePlayerSort sendProto;
	sendProto.set_ranktype(proLists.ranktype());
	sendProto.set_char_id(nCharID);
	sendProto.set_sort(myRankScore.first);
	sendProto.set_value(myRankScore.second);

	pSession->sendMsgProto(::comdef::msg_ss, ::msg_maj::UpdateRankSortResponseID,pMsg->clientSessID,pMsg->fepServerID, sendProto);
}

void ProcSsHandler::OnQueryRankRequest(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CSortsManager::Instance()->SendTopList(pSession, pMsg, nSize);
}

void ProcSsHandler::OnAddRoomToWs(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	::msg_maj::AddRoomToWs proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);
	GameService::getMe().GetSceneRoomMgr()->addRoom(proto.room_id(), proto.room_info());
}

void ProcSsHandler::OnUpdateRoomToWs(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	::msg_maj::UpdateRoomToWs proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);
	SceneRoom* pRoom = GameService::getMe().GetSceneRoomMgr()->getRoom(proto.room_id());
	if (pRoom)
	{
		std::vector< ::msg_maj::RoleInfo> roles;
		for (size_t i = 0; i < proto.role_list_size(); ++i)
		{
			roles.push_back(proto.role_list(i));
		}
		pRoom->UpdateRoles(roles);
	}
}

void ProcSsHandler::OnRemoveRoomToWs(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	::msg_maj::RemoveRoomToWs proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);
	GameService::getMe().GetSceneRoomMgr()->removeRoom(proto.room_id());
}

void ProcSsHandler::OnSyncRoleToWs(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	::msg_maj::SyncRoleToWs proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);
	uint64_t userId = proto.uid();
	WorldUser* wsUser = GameService::getMe().GetWorldUserMgr()->getByUID(userId);
	if (wsUser == NULL)
	{
		return;
	}
	wsUser->UpdateRoleFromSs(proto);
}

void ProcSsHandler::OnReqRobotJoinRoom(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	::msg_maj::ReqRobotJoinRoom proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);

	struct exceOfflineUser : execEntry<OfflineUser>
	{
		exceOfflineUser()
		{
			vecUserID.clear();
		}

		virtual bool exec(OfflineUser *entry)
		{
			if (entry->GetID() >= ROBOT_START_ID && entry->GetID() <= ROBOT_END_ID)
			{
				WorldUser* wsUser = GameService::getMe().GetWorldUserMgr()->getByUID(entry->GetID());
				if (wsUser)
				{
					if (wsUser->GetRoomID() < 1)
					{
						vecUserID.push_back(wsUser->GetID());
					}
				}
				else
				{
					vecUserID.push_back(entry->GetID());
				}
			}
			return true;
		}
		std::vector<uint64_t> vecUserID;
	};

	exceOfflineUser exec;
	GameService::getMe().GetOfflineUserMgr()->execEveryUser(exec);

	if (exec.vecUserID.empty())
	{
		H::logger->error("找不到任何的机器去加入房间");
		return;
	}

	db::DBConnection* dbConn = GameService::getMe().GetDataRef();
	if (dbConn == NULL)
	{
		ASSERT(0);
		return;
	}

	int randIndex = randBetween(0, exec.vecUserID.size() - 1);
	uint64_t userID = exec.vecUserID[randIndex];

	std::stringstream dataWsSql;
	dataWsSql << "select `id`,`account`,`nickname`,`logo_icon`,`room_card`,`total_games`,`win_games`,`his_max_con`,`his_max_score`,`top_score_total` from `tb_role` WHERE id=" << userID << ";";
	::msg_maj::RoleWs roleWs;
	int ret = doQueryProto(*dbConn, dataWsSql.str(), roleWs);
	if (ret != 0)
	{
		ASSERT(0);
		return ;
	}

	WorldUser* wsUser = GameService::Instance()->GetWorldUserMgr()->AddUser(roleWs, userID);
	if (wsUser == NULL) // 发现有相同的角色ID，踢掉旧的session
	{
		ASSERT(0);
		return;
	}

	zSession* fepSession = GameService::Instance()->SessionMgr()->getFep();

	std::vector<int32_t> game_list;
	game_list.push_back(4000);
	game_list.push_back(4001);
	game_list.push_back(4002);

	wsUser->SetFepSession(fepSession);
	wsUser->SetSceneSession(pSession);
	wsUser->refreshProxyServerList(game_list);

	::msg_maj::LoginToScene sendLogin;
	sendLogin.set_uid(wsUser->GetID());
	sendLogin.set_repeat_login(false);
	sendLogin.set_new_session_id(0);
	sendLogin.set_join_room_id(proto.room_id());
	wsUser->sendMsgToSs(::comdef::msg_ss, ::msg_maj::login_to_scene_req, sendLogin);

}

void ProcSsHandler::OnReqBindInfoReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	fogs::proto::msg::GetBindingAgentInfoReq proData;
	proData.ParseFromArray(pMsg->data, pMsg->size);

	const zServerMgr* pSrvMgr = GameService::getMe().SrvSerivceMgr()->GetServerMgr(GameService::getMe().GetServerID());
	if (pSrvMgr == NULL)
	{
		H::logger->error("Not found zServerMgr");
		return;
	}

	WorldUser* user = GameService::Instance()->GetWorldUserMgr()->getByUID(proData.user_id());
	if (user == NULL)
	{
		ASSERT(0);
		H::logger->error("Not found user");
		return;
	}

	std::string platUrl = pSrvMgr->GetPlatUrl();
	char destUrl[1024] = { 0 };
	sprintf(destUrl, "%s/onFindBindInfo?uid=%lld", pSrvMgr->GetPlatUrl().c_str(), user->GetID());

	CurlParm* parm = new CurlParm();
	parm->conn = pSession;
	parm->account = user->GetAccID();
	parm->sessionID = pMsg->clientSessID;

	CURL *curl;
	CURLcode res;
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();    // 初始化
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, destUrl);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &onFindBindInfo);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, parm);
		res = curl_easy_perform(curl);   // 执行
		curl_easy_cleanup(curl);
	}
	curl_global_cleanup();


}

size_t onFindBindInfo(void *buffer, size_t size, size_t nmemb, void *parm)
{
	CurlParm* pParm = (CurlParm*)parm;
	if (NULL == pParm)
	{
		return 0;
	}

	int32_t dataLength = (size * nmemb);
	pParm->buff_times++;
	memcpy(&pParm->buffer[pParm->buff_length], (char*)buffer, dataLength);
	pParm->buff_length += dataLength;

	std::string jsonStrSrc((char*)pParm->buffer, pParm->buff_length);
	zSession* pSession = pParm->conn;
	std::string account = pParm->account;
	uint32_t clientSessionID = pParm->sessionID;

	delete pParm;
	pParm = NULL;

	JSON::Parser parser;
	Dynamic::Var result;
	parser.reset();

	result = parser.parse(jsonStrSrc);
	JSON::Object::Ptr pObj = result.extract<JSON::Object::Ptr>();

	fogs::proto::msg::GetBindingAgentInfoResp proData;
	proData.set_agent_id(atoi(pObj->get("agent_id").toString().c_str()));
	proData.set_agent_name(pObj->get("agent_name").toString());
	proData.set_agent_icon(pObj->get("agent_icon").toString());

	pSession->sendMsgProto(::comdef::msg_ss, ::msg_maj::GetBindingAgentInfoRespID, clientSessionID, proData);

	return dataLength;

}

void ProcSsHandler::OnReqBindAgentReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{

	fogs::proto::msg::BindingAgentReq proData;
	proData.ParseFromArray(pMsg->data, pMsg->size);

	const zServerMgr* pSrvMgr = GameService::getMe().SrvSerivceMgr()->GetServerMgr(GameService::getMe().GetServerID());
	if (pSrvMgr == NULL)
	{
		H::logger->error("Not found zServerMgr");
		return;
	}

	WorldUser* user = GameService::Instance()->GetWorldUserMgr()->getByUID(proData.user_id());
	if (user == NULL)
	{
		ASSERT(0);
		H::logger->error("Not found user");
		return;
	}


	std::string platUrl = pSrvMgr->GetPlatUrl();
	char destUrl[1024] = { 0 };
	sprintf(destUrl, "%s/onBindingAgent?uid=%lld&from_uid=%d", pSrvMgr->GetPlatUrl().c_str(), user->GetID(), proData.agent_id());

	CurlParm* parm = new CurlParm();
	parm->conn = pSession;
	parm->account = user->GetAccID();
	parm->sessionID = pMsg->clientSessID;

	CURL *curl;
	CURLcode res;
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();    // 初始化
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, destUrl);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &onBindingAgent);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, parm);
		res = curl_easy_perform(curl);   // 执行
		curl_easy_cleanup(curl);
	}
	curl_global_cleanup();
}

size_t onBindingAgent(void *buffer, size_t size, size_t nmemb, void *parm)
{
	CurlParm* pParm = (CurlParm*)parm;
	if (NULL == pParm)
	{
		return 0;
	}

	int16_t dataLength = (size * nmemb);
	pParm->buff_times++;
	memcpy(&pParm->buffer[pParm->buff_length], (char*)buffer, dataLength);
	pParm->buff_length += dataLength;

	std::string jsonStrSrc((char*)pParm->buffer, pParm->buff_length);
	zSession* pSession = pParm->conn;
	std::string account = pParm->account;
	uint32_t clientSessionID = pParm->sessionID;

	delete pParm;
	pParm = NULL;

	JSON::Parser parser;
	Dynamic::Var result;
	parser.reset();

	result = parser.parse(jsonStrSrc);
	JSON::Object::Ptr pObj = result.extract<JSON::Object::Ptr>();

	fogs::proto::msg::BindingAgentResp proData;
	proData.set_result(atoi(pObj->get("result").toString().c_str()));
	proData.set_user_id(atoi(pObj->get("user_id").toString().c_str()));
	proData.set_agent_id(atoi(pObj->get("agent_id").toString().c_str()));
	proData.set_agent_name(pObj->get("agent_name").toString());
	proData.set_agent_icon(pObj->get("agent_icon").toString());
	proData.set_code_info(pObj->get("code_info").toString());

	pSession->sendMsgProto(::comdef::msg_ss, ::msg_maj::BindingAgentRespID, clientSessionID, proData);

	return dataLength;
}

void ProcSsHandler::OnInputInviteCodeReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	fogs::proto::msg::InputInviteCodeReq proData;
	proData.ParseFromArray(pMsg->data, pMsg->size);

	const zServerMgr* pSrvMgr = GameService::getMe().SrvSerivceMgr()->GetServerMgr(GameService::getMe().GetServerID());
	if (pSrvMgr == NULL)
	{
		H::logger->error("Not found zServerMgr");
		return;
	}

	WorldUser* user = GameService::Instance()->GetWorldUserMgr()->getByUID(proData.user_id());
	if (user == NULL)
	{
		ASSERT(0);
		H::logger->error("Not found user");
		return;
	}

	std::string platUrl = pSrvMgr->GetPlatUrl();
	char destUrl[1024] = { 0 };
	sprintf(destUrl, "%s/onExchange?uid=%lld&code=%s", pSrvMgr->GetPlatUrl().c_str(), user->GetID(), proData.code().c_str());

	CurlParm* parm = new CurlParm();
	parm->conn = pSession;
	parm->account = user->GetAccID();
	parm->sessionID = pMsg->clientSessID;

	CURL *curl;
	CURLcode res;
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();    // 初始化
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, destUrl);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &onInputInviteCode);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, parm);
		res = curl_easy_perform(curl);   // 执行
		curl_easy_cleanup(curl);
	}
	curl_global_cleanup();
}

size_t onInputInviteCode(void *buffer, size_t size, size_t nmemb, void *parm)
{
	CurlParm* pParm = (CurlParm*)parm;
	if (NULL == pParm)
	{
		return 0;
	}

	int32_t dataLength = (size * nmemb);
	pParm->buff_times++;
	memcpy(&pParm->buffer[pParm->buff_length], (char*)buffer, dataLength);
	pParm->buff_length += dataLength;

	std::string jsonStrSrc((char*)pParm->buffer, pParm->buff_length);
	zSession* pSession = pParm->conn;
	std::string account = pParm->account;
	uint32_t clientSessionID = pParm->sessionID;

	delete pParm;
	pParm = NULL;

	JSON::Parser parser;
	Dynamic::Var result;
	parser.reset();

	result = parser.parse(jsonStrSrc);
	JSON::Object::Ptr pObj = result.extract<JSON::Object::Ptr>();

	fogs::proto::msg::InputInviteCodeResp proData;
	proData.set_result(atoi(pObj->get("result").toString().c_str()));
	proData.set_room_card(atoi(pObj->get("room_card").toString().c_str()));
	proData.set_code_info(pObj->get("code_info").toString());
	
	pSession->sendMsgProto(::comdef::msg_ss, ::msg_maj::InputInviteCodeRespID, clientSessionID, proData);

	return dataLength;
}

void ProcSsHandler::RqSceneRegister(zSession* pSession, const PbMsgWebSS* pMsg,int32_t nSize)
{

}

void ProcSsHandler::RqSceneCancel(zSession* pSession, const PbMsgWebSS* pMsg,int32_t nSize)
{
	
}

void ProcSsHandler::RpEnterSceneResult(zSession* pSession, const PbMsgWebSS* pMsg,int32_t nSize)
{
	
}

void ProcSsHandler::RpChangeScene(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	
}


