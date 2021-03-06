#include "CMajingRuleLC.hpp"

#include "CRoom.hpp"
#include "CPlayer.hpp"
#include "CMaJiang.hpp"
#include "CHuScore.h"


CMajingRuleLC::CMajingRuleLC(CRoom* pRoom, const ::msg_maj::RoomOption& roomOption)
	: CMajingRule(pRoom,roomOption)
{
	const ::msg_maj::LCOption& option = m_roomOption.lc_option();
	m_usMaiMaType = option.maimatype();	// 买马类型
	m_bGhost = HAS_FLAG(option.wanfatype(), WAN_FA_MASK_GHOST);
	m_bDaHuJiaBei = HAS_FLAG(option.wanfatype(), WAN_FA_MASK_DaHuJiaBei);
	m_bCanPaoHu = HAS_FLAG(option.wanfatype(), WAN_FA_MASK_CanPaoHu);
	m_bGangShangPaoBao3Jia = HAS_FLAG(option.wanfatype(), WAN_FA_MASK_GangShangPaoBao3Jia);
	m_bQiangGangBao3Jia = HAS_FLAG(option.wanfatype(), WAN_FA_MASK_QiangGangBao3Jia);
	m_bGangKaiHuaIsBao3Jia = HAS_FLAG(option.wanfatype(), WAN_FA_MASK_GangKaiHuaBao3Jia);

	if (IsGhost())
	{
		AddGhostPai(7);
	}
}

CMajingRuleLC::~CMajingRuleLC()
{

}

uint16_t CMajingRuleLC::GetBankerSeat()
{
	const vecHuPai& vecHuPai = m_pMaj->GetVecHuPai();
	switch (vecHuPai.size())
	{
	case 0:
	{
		if (m_pRoom->GetTotalPersons() > 0)
		{
			return (m_pRoom->GetBankerSeat() + 1) % m_pRoom->GetTotalPersons();
		}

		break;
	}
	case 1: return vecHuPai[0].m_usHupaiPos; break;
	default: return m_pMaj->GetUsCurActionPos(); break;
	}
	return m_pRoom->GetBankerSeat();
}

// 检测平胡
bool CMajingRuleLC::CheckHupai_PingHu(const std::vector<uint16_t>& pailist)
{
	return CMajingRule::CheckHupai_PingHu(pailist);
}

// 检测胡牌
::msg_maj::hu_type CMajingRuleLC::CheckHupaiAll(CPlayer* pPlayer, const std::vector<uint16_t>& pailist, const vecEventPai& eventpailist, uint16_t usTingPai)
{
	return CheckHupaiAndGhost(pPlayer, pailist, eventpailist, usTingPai);
}

// 检测胡牌
::msg_maj::hu_type CMajingRuleLC::CheckHupaiAndGhost(CPlayer* pPlayer, const std::vector<uint16_t>& pailist, const vecEventPai& eventpailist, uint16_t usTingPai)
{
	Display(pailist);
	if (pailist.size() % 3 != 2)
	{
		return ::msg_maj::hu_none;
	}

	std::multimap<int16_t, ::msg_maj::hu_type, std::greater<int16_t> > mapScoreType;
	uint16_t score = 0;
	::msg_maj::hu_type huType = ::msg_maj::hu_none;
	bool bRet = m_bGhost ? CheckHupai_QiDui_Ghost(pailist) : CheckHupai_QiDui(pailist);
	if (bRet)
	{
		huType = ::msg_maj::hu_t_lc_qidui;
		score = CHuScore::Instance()->GetPaiXingScore(huType);
		mapScoreType.insert(std::make_pair(score, huType));
		if (IsQys(pailist, eventpailist, m_bGhost))
		{
			huType = ::msg_maj::hu_t_lc_qysqd;
			score = CHuScore::Instance()->GetPaiXingScore(huType);
			mapScoreType.insert(std::make_pair(score, huType));
		}
	}

	if (huType == ::msg_maj::hu_none)
	{
		bool bCanPingHu = m_bGhost ? CheckHupai_PingHu_Ghost(pailist) : CheckHupai_PingHu(pailist);
		if (bCanPingHu)
		{
			huType = ::msg_maj::hu_t_lc_pinghu;
			score = CHuScore::Instance()->GetPaiXingScore(huType);
			mapScoreType.insert(std::make_pair(score, huType));
			if (IsPPHu(pailist, eventpailist, m_bGhost))
			{
				huType = ::msg_maj::hu_t_lc_pphu;
				score = CHuScore::Instance()->GetPaiXingScore(huType);
				mapScoreType.insert(std::make_pair(score, huType));
			}
			if (IsQys(pailist, eventpailist, m_bGhost))
			{
				huType = ::msg_maj::hu_t_lc_qys;
				score = CHuScore::Instance()->GetPaiXingScore(huType);
				mapScoreType.insert(std::make_pair(score, huType));
				if (IsPPHu(pailist, eventpailist, m_bGhost))
				{
					huType = ::msg_maj::hu_t_lc_qyspphu;
					score = CHuScore::Instance()->GetPaiXingScore(huType);
					mapScoreType.insert(std::make_pair(score, huType));
				}
			}
		}
	}

	if (huType != ::msg_maj::hu_none)
	{
		if (IsDiHu(pPlayer))
		{
			huType = ::msg_maj::hu_t_lc_dihu;
			score = CHuScore::Instance()->GetPaiXingScore(huType);
			mapScoreType.insert(std::make_pair(score, huType));
		}

		if (IsTianHu(pPlayer))
		{
			huType = ::msg_maj::hu_t_lc_tianhu;
			score = CHuScore::Instance()->GetPaiXingScore(huType);
			mapScoreType.insert(std::make_pair(score, huType));
		}

		if (mapScoreType.empty())
		{
			return ::msg_maj::hu_none;
		}
		else
		{
			return mapScoreType.begin()->second;
		}
	}
	return huType;
}

// 杠分在最后的胡牌才能结算 
void CMajingRuleLC::CountEventMingGang(uint16_t usSeat)
{
	//胡牌基础分数
	m_pRoom->GetPlayer(usSeat)->AddMingGangTimes(1);
	m_pRoom->GetPlayer(m_pMaj->GetUsCurActionPos())->AddFangGangTimesThisInn(1);
	m_pRoom->GetPlayer(m_pMaj->GetUsCurActionPos())->AddFangGangTimes(1);
}

void CMajingRuleLC::CountEventAnGang(uint16_t usSeat)
{
	m_pRoom->GetPlayer(usSeat)->AddAnGangTimes(1);
}

void CMajingRuleLC::CountEventGouShouGang(uint16_t usSeat)
{
	m_pRoom->GetPlayer(usSeat)->AddNextGangTimes(1);
}

bool CMajingRuleLC::CanDianPao(CPlayer* pPlayer)
{
	return CMajingRule::CanDianPao(pPlayer);
}

bool CMajingRuleLC::IsCanCountGang() 
{ 
	return !m_pMaj->IsNotHued();
}

void CMajingRuleLC::CountResult()
{
	// 总分 = 牌形分 * 胡牌方式
	CMaJiang* pMaj = m_pMaj;
	vecHuPai huPaies = pMaj->GetVecHuPai();
	std::pair<uint16_t, uint16_t> pairhitMaInfo;
	for (vecHuPai::const_iterator it = huPaies.begin(); it != huPaies.end(); ++it)
	{
		const stHuPai& sthupai = *it;

		// 增加其他记录
		CPlayer* pHuPlayer = m_pRoom->GetPlayer(sthupai.m_usHupaiPos);
		if (pHuPlayer == NULL)
		{
			continue;
		}

		switch (sthupai.m_eHupaiWay)
		{
		case ::msg_maj::hu_way_lc_zimo:
		case ::msg_maj::hu_way_lc_gangkaihua:
		case ::msg_maj::hu_way_lc_dianpao:
		{
			// 结算胡牌人的分
			CountHupaiScore(sthupai, pHuPlayer->GetPaiList(), pHuPlayer->GetEventPaiList(), pairhitMaInfo);
			pHuPlayer->AddZiMoTimes(1, pHuPlayer->IsGhostHu(this));
			pHuPlayer->AddHitMaTotal(pairhitMaInfo.first);
			pHuPlayer->AddHuPaiTotal(1);
			break;
		}
		case ::msg_maj::hu_way_lc_gangshangpao:
		case ::msg_maj::hu_way_lc_qiangganghu:
		{
			CountHupaiScore(sthupai, pHuPlayer->GetPaiList(), pHuPlayer->GetEventPaiList(), pairhitMaInfo);
			pHuPlayer->AddHitMaTotal(pairhitMaInfo.first);
			pHuPlayer->AddHuPaiTotal(1);
			break;
		}
		default:
			break;
		}
	}
}

void CMajingRuleLC::CountGangResult()
{
	vecHuPai huPaies = m_pMaj->GetVecHuPai();
	std::map<uint16_t, int16_t> o_mapScore;
	for (uint16_t i = 0; i < m_pRoom->GetTotalPersons(); ++i)
	{
		bool find = false;
		stHuPai sthupai;
		sthupai.m_usHupaiPos = -1;
		for (vecHuPai::const_iterator it = huPaies.begin(); it != huPaies.end(); ++it)
		{
			if (it->m_usHupaiPos == i)
			{
				sthupai = *it;
				find = true;
				break;
			}
		}
		if (!find)
		{
			sthupai.m_eHupaiType = msg_maj::hu_none;
			sthupai.m_eHupaiWay = msg_maj::hu_way_none;
			sthupai.m_usHupaiPos = i;
		}
		CountGangScore(sthupai, i);
	}
}

bool CMajingRuleLC::CanBuGang(CPlayer* pPlayer, std::vector<uint16_t>& agpailist)
{
	if (NULL == pPlayer)
	{
		//LOG(ERROR) << "CanBuGang() NULL == pPlayer";
		return false;
	}

	const std::vector<uint16_t>& paiList = pPlayer->GetPaiList();
	for (std::vector<uint16_t>::const_iterator it = paiList.begin(); it != paiList.end(); ++it)
	{
		if (pPlayer->CheckNextGang(*it))
		{
			agpailist.push_back(*it);
		}
	}
	return !agpailist.empty();
}

void CMajingRuleLC::GetHitMaDatas(const stHuPai& sthupai, uint16_t& hitMaTimes)
{
	hitMaTimes = 0;
}

int32_t CMajingRuleLC::CountHupaiBaseScore(const stHuPai& sthupai, const std::vector<uint16_t>& pailist, const vecEventPai& eventpailist, stHuDetail& huDetail)
{
	// 是否作大胡
	int32_t usBigHuMulti = m_bDaHuJiaBei ? CHuScore::Instance()->GetPaiXingScore(sthupai.m_eHupaiType) : 1;
	int32_t usExtraMulti = 0;
	int32_t item3 = 0;
	int32_t item4 = 0;
	int32_t item5 = 0;
	int32_t item6 = 0;
	switch (sthupai.getDefHuWay())
	{
	case ::msg_maj::hu_way_gangkaihua:item3 = 1; usExtraMulti += 2; break;
	case ::msg_maj::hu_way_qiangganghu:item4 = 1; usExtraMulti += 2; break;
	case ::msg_maj::hu_way_gangshangpao:item5 = 1; usExtraMulti += 2; break;
	default:
		break;
	}

	// 无鬼加倍
	if (IsGhost() && !IsHasGhostPai(pailist))
	{
		item6 = 1;
		usExtraMulti += 2;
	}

	int32_t usMaiMaCount = 0;
	CPlayer* pPlayer = m_pRoom->GetPlayer(sthupai.m_usHupaiPos);
	if (pPlayer)
	{
		CheckHitMa(sthupai.m_usHupaiPos, pPlayer->GetHitMa());
		usMaiMaCount = pPlayer->GetHitMa().size();
	}

	int32_t usBaseScore = GetBaseScore();
	int32_t usTotalBaseScore = usBaseScore * usBigHuMulti;
	if (usExtraMulti)
	{
		usTotalBaseScore *= usExtraMulti;
	}

	if (usMaiMaCount)
	{
		usTotalBaseScore *= (usMaiMaCount + 1);
	}

	huDetail.item1 = usBaseScore;
	huDetail.item2 = usBigHuMulti;
	huDetail.item3 = item3;
	huDetail.item4 = item4;
	huDetail.item5 = item5;
	huDetail.item6 = item6;
	huDetail.item10 = usExtraMulti;
	huDetail.item11 = usMaiMaCount;

	// 接炮 = 底分 * 大胡牌型番数 *  特殊胡牌番数 * (码数  + 1) + 杠分
	// 自摸 = 底分 * 大胡牌型番数 *  特殊胡牌番数 * (码数  + 1) + 杠分
	return usTotalBaseScore;
}

void CMajingRuleLC::CountHupaiScore(const stHuPai& sthupai, const std::vector<uint16_t>& pailist, const vecEventPai& eventpailist, std::pair<uint16_t, uint16_t>& out_mainfo)
{
	uint32_t nMaxMultiNum = m_pRoom->GetMaxMultiNum();

	stHuDetail stDetail;
	int32_t nTotalBaseScore = CountHupaiBaseScore(sthupai, pailist, eventpailist, stDetail);
	if (nTotalBaseScore == 0)
	{
		return;
	}

	CPlayer* huPlayer = m_pRoom->GetPlayer(sthupai.m_usHupaiPos);
	if (huPlayer == NULL)
	{
		return;
	}

	std::vector<int16_t> doedMultiSeat;
	int16_t winScore = 0;
	for (uint16_t i = 0; i < m_pRoom->GetTotalPersons(); ++i)
	{
		if (NULL == m_pRoom->GetPlayer(i))  continue;

		if (i == sthupai.m_usHupaiPos) continue;

		bool isDianPao = (sthupai.m_eHupaiWay == ::msg_maj::hu_way_lc_dianpao
			|| sthupai.m_eHupaiWay == ::msg_maj::hu_way_lc_gangshangpao
			|| sthupai.m_eHupaiWay == ::msg_maj::hu_way_lc_qiangganghu);

		int32_t nScoreTmp = isDianPao ? nTotalBaseScore : (nTotalBaseScore * 2);
		nScoreTmp = nScoreTmp < nMaxMultiNum ? nScoreTmp : nMaxMultiNum;
		winScore += nScoreTmp;

		switch (sthupai.m_eHupaiWay)
		{
		case ::msg_maj::hu_way_lc_zimo:
		{
			m_pRoom->GetPlayer(i)->AddTotalFan(-nScoreTmp);
			int16_t offsetSeat = getOffsetPos(sthupai.m_usHupaiPos, i);
			doedMultiSeat.push_back(offsetSeat);
			break;
		}
		case ::msg_maj::hu_way_lc_gangshangpao:
		case ::msg_maj::hu_way_lc_dianpao:
		{
			if (i == m_pMaj->GetUsCurActionPos())
			{
				m_pRoom->GetPlayer(i)->AddTotalFan(-nScoreTmp);
				int16_t offsetSeat = getOffsetPos(sthupai.m_usHupaiPos, i);
				doedMultiSeat.push_back(offsetSeat);
			}
			else
			{
				winScore -= nScoreTmp;
				continue;
			}
			break;
		}
		case ::msg_maj::hu_way_lc_qiangganghu: //抢杠胡
		{
			if (IsGangKaiHuaBao3Jia(sthupai)) // 勾先包三家，不勾，只有一家
			{
				m_pRoom->GetPlayer(m_pMaj->m_nLastGangPos)->AddTotalFan(-nScoreTmp);
				int16_t offsetSeat = getOffsetPos(sthupai.m_usHupaiPos, m_pMaj->m_nLastGangPos);
				doedMultiSeat.push_back(offsetSeat);
			}
			else
			{
				if (i == m_pMaj->m_nLastGangPos)
				{
					m_pRoom->GetPlayer(i)->AddTotalFan(-nScoreTmp);
					int16_t offsetSeat = getOffsetPos(sthupai.m_usHupaiPos, i);
					doedMultiSeat.push_back(offsetSeat);
				}
				else
				{
					winScore -= nScoreTmp;
				}
			}
			break;
		}
		case ::msg_maj::hu_way_lc_gangkaihua: // 
		{
			m_pRoom->GetPlayer(i)->AddTotalFan(-nScoreTmp);
			int16_t offsetSeat = getOffsetPos(sthupai.m_usHupaiPos, i);
			doedMultiSeat.push_back(offsetSeat);
			break;
		}
		default:
		{
			m_pRoom->GetPlayer(i)->AddTotalFan(-nScoreTmp);
			int16_t offsetSeat = getOffsetPos(sthupai.m_usHupaiPos, i);
			doedMultiSeat.push_back(offsetSeat);
			break;
		}
		}
	}
	stDetail.doedMultiSeat = doedMultiSeat;
	m_pRoom->GetPlayer(sthupai.m_usHupaiPos)->AddTotalFan(winScore);
	stDetail.SetData(sthupai.m_usHupaiPos, sthupai.m_usHupaiPos, getOffsetPos(sthupai.m_usHupaiPos, -1), winScore, sthupai);
	m_pRoom->GetPlayer(sthupai.m_usHupaiPos)->AddScore(stDetail);

	out_mainfo.first = 0;
	out_mainfo.second = winScore;
}

void CMajingRuleLC::CountGangScore(const stHuPai& sthupai, uint16_t gangSeat)
{
	int32_t usDifenBase = GetBaseScore();
	int32_t nTotalGangScore = m_pRoom->GetPlayer(gangSeat)->GetAnGangTimes() * usDifenBase * 2;
	nTotalGangScore += m_pRoom->GetPlayer(gangSeat)->GetNextGangTimes();
	if (nTotalGangScore > 0)
	{
		int32_t winScore = 0;
		for (uint16_t j = 0; j < m_pRoom->GetTotalPersons(); ++j)
		{
			if (gangSeat == j) continue;

			switch (sthupai.m_eHupaiWay)
			{
			case ::msg_maj::hu_way_lc_qiangganghu: //抢杠胡
			{
				if (false) // 勾先包三家，不勾，只有一家
					m_pRoom->GetPlayer(m_pMaj->m_nLastGangPos)->AddTotalFan(-nTotalGangScore);
				else
					m_pRoom->GetPlayer(j)->AddTotalFan(-nTotalGangScore);
				break;
			}
			case ::msg_maj::hu_way_lc_gangkaihua: // 暗杠(无三家),其他杠(有可能包三家)
			{
				if (false)	// 如果是明杠杠上花，则有可能要包三家
					m_pRoom->GetPlayer(m_pMaj->m_nLastGangFromPos)->AddTotalFan(-nTotalGangScore);
				else
					m_pRoom->GetPlayer(j)->AddTotalFan(-nTotalGangScore);
				break;
			}
			default:
			{
				m_pRoom->GetPlayer(j)->AddTotalFan(-nTotalGangScore);
				break;
			}
			}
			winScore += nTotalGangScore;
		}
		m_pRoom->GetPlayer(gangSeat)->AddTotalFan(winScore);
	}

	// 明杠
	uint32_t nWinTotal = 0;
	const std::vector<uint16_t>& seated = m_pRoom->GetPlayer(gangSeat)->GetMingGangSeated();
	for (std::vector<uint16_t>::const_iterator it2 = seated.begin(); it2 != seated.end(); ++it2)
	{
		uint32_t tmpScoer = usDifenBase * (m_pRoom->GetTotalPersons() - 1);
		nWinTotal += tmpScoer;
		switch (sthupai.m_eHupaiWay)
		{
		case ::msg_maj::hu_way_lc_qiangganghu: //抢杠胡
		{
			if (false) // 勾先包三家，不勾，只有一家
				m_pRoom->GetPlayer(m_pMaj->m_nLastGangPos)->AddTotalFan(-tmpScoer);
			else
				m_pRoom->GetPlayer(*it2)->AddTotalFan(-tmpScoer);
			break;
		}
		case ::msg_maj::hu_way_lc_gangkaihua: // 暗杠(无三家),其他杠(有可能包三家)
		{
			if (false)	// 如果是明杠杠上花，则有可能要包三家
				m_pRoom->GetPlayer(m_pMaj->m_nLastGangFromPos)->AddTotalFan(-tmpScoer);
			else
				m_pRoom->GetPlayer(*it2)->AddTotalFan(-tmpScoer);
			break;
		}
		default:
		{
			m_pRoom->GetPlayer(*it2)->AddTotalFan(-tmpScoer);
			break;
		}
		}
	}
	m_pRoom->GetPlayer(gangSeat)->AddTotalFan(nWinTotal);
}

void CMajingRuleLC::SetHuInfo(const stHuPai& sthupai, uint16_t seat, ::msg_maj::HuInfo* huInfo, bool isFull)
{
	huInfo->set_game_type(::msg_maj::maj_t_yulin);
	huInfo->mutable_lc_info()->set_hutype(::msg_maj::hu_type(sthupai.m_eHupaiType));
	huInfo->mutable_lc_info()->set_huway(::msg_maj::hu_way(sthupai.m_eHupaiWay));
	if (!isFull)
	{
		return;
	}

	uint16_t i = seat;

	for (std::vector<uint16_t>::const_iterator iter = m_pMaj->GetMaPaiList().begin(); iter != m_pMaj->GetMaPaiList().end(); ++iter)
	{
		huInfo->mutable_lc_info()->add_ma_pai_all(*iter);
	}
	for (std::vector<uint16_t>::iterator iter = m_pRoom->GetPlayer(i)->GetHitMa().begin(); iter != m_pRoom->GetPlayer(i)->GetHitMa().end(); ++iter)
	{
		huInfo->mutable_lc_info()->add_ma_pai_hit(*iter);
	}
}

void CMajingRuleLC::SetResultSeatHuInfo(const stGameResultSeat& seatData, ::msg_maj::HuInfo* huInfo)
{
	huInfo->set_game_type(::msg_maj::maj_t_yulin);
	huInfo->mutable_lc_info()->set_huway(seatData.hu_info.hu_way);
	huInfo->mutable_lc_info()->set_hutype(seatData.hu_info.hu_type);
	for (std::vector<int16_t>::const_iterator it = seatData.hu_info.ma_pai_all.begin(); it != seatData.hu_info.ma_pai_all.end(); ++it)
	{
		huInfo->mutable_lc_info()->add_ma_pai_all(*it);
	}

	for (std::vector<int16_t>::const_iterator it = seatData.hu_info.ma_pai_hit.begin(); it != seatData.hu_info.ma_pai_hit.end(); ++it)
	{
		huInfo->mutable_lc_info()->add_ma_pai_hit(*it);
	}
}

void CMajingRuleLC::SetReplayActionHuInfo(const stReplayAction& actionData, ::msg_maj::HuInfo* huInfo)
{
	huInfo->set_game_type(::msg_maj::maj_t_yulin);
	huInfo->mutable_lc_info()->set_huway(actionData.hu_info.hu_way);
	huInfo->mutable_lc_info()->set_hutype(actionData.hu_info.hu_type);
	for (int j = 0; j < actionData.hu_info.ma_pai_all.size(); ++j)
	{
		huInfo->mutable_lc_info()->add_ma_pai_all(actionData.hu_info.ma_pai_all[j]);
	}
	for (int j = 0; j < actionData.hu_info.ma_pai_hit.size(); ++j)
	{
		huInfo->mutable_lc_info()->add_ma_pai_hit(actionData.hu_info.ma_pai_hit[j]);
	}
}

bool CMajingRuleLC::IsThisInnMyWin(CPlayer* pPlayer) const
{
	if (pPlayer->IsHued() && m_pMaj->m_usFirstSeat == pPlayer->GetSeat())
	{
		return true;
	}
	return false;
}

bool CMajingRuleLC::IsGuoPengThisPai(CPlayer* pPlayer, uint16_t usPai)
{
	if (IsGuoPeng())
	{
		std::set<uint16_t>::const_iterator it = pPlayer->m_setGuoPengPai.find(usPai);
		if (it != pPlayer->m_setGuoPengPai.end())
		{
			if (usPai == *it)
			{
				return true;
			}
		}
	}
	return false;
}

eRoomStatus CMajingRuleLC::AcceptAskAllNextStatus() const
{ 
	return eRoomStatus_StartGame; 
}

eRoomStatus CMajingRuleLC::SendHandCardsAllNextState() const
{
	return eRoomStatus_StartGame;
}

eRoomStatus CMajingRuleLC::DisoverCardAllCheckAndDoEvent() const
{
	return eRoomStatus_StartGame;
}



