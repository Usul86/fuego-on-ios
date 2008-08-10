//----------------------------------------------------------------------------
/** @file GoUctGlobalSearch.h
*/
//----------------------------------------------------------------------------

#ifndef GOUCT_GLOBALSEARCH_H
#define GOUCT_GLOBALSEARCH_H

#include "GoBoard.h"
#include "GoBoardUtil.h"
#include "GoEyeUtil.h"
#include "GoRegionBoard.h"
#include "GoSafetySolver.h"
#include "GoUctSearch.h"
#include "GoUctUtil.h"

//----------------------------------------------------------------------------

/** Enable the usage of the safety solver (currently not functional).
    Compile-time constant to enable the usage of the safety solver. This
    cost some performance and there are no experiments yet showing that it
    improves the playing strength. It is also currently not functional,
    since it has not been updated after code changes.
*/
const bool GOUCT_USE_SAFETY_SOLVER = false;

//----------------------------------------------------------------------------

#if 0

// TODO: GoUctPlayoutPolicy was previously a base class, not it is a
// template parameter if GouctGlobalSearch. GoUctPlayoutPolicy should become
// a concept (Boost.Concept) that checks for the reuqired functions in the
// template parameter

/** Policy for the random play-out phase of GoUctGlobalSearch. */
template<class BOARD>
class GoUctPlayoutPolicy
{
public:
    GoUctPlayoutPolicy(const BOARD& bd);

    virtual ~GoUctPlayoutPolicy();

    /** Called after playout.
        Default implementation does nothing.
    */
    virtual void EndPlayout();

    /** Called at beginning of playout.
        Default implementation does nothing.
    */
    virtual void StartPlayout();

    /** Called after a move eas played
        Default implementation does nothing.
    */
    virtual void OnPlay();

    /** Generate a move in the random play-out phase of GoUctSearch.
        The policy is not allowed to return SG_PASS, as long as there are
        still points on the board, for which GoUctUtil::GeneratePoint
        returns true, because GoUctSearch relies on the fact that it can
        score positions with GoBoardUtil::ScoreSimpleEndPosition after
        two consecutive passes.
    */
    virtual SgPoint GenerateMove() = 0;

protected:
    const BOARD& Board() const;

private:
    const BOARD& m_bd;
};

template<class BOARD>
inline GoUctPlayoutPolicy<BOARD>::GoUctPlayoutPolicy(const BOARD& bd)
    : m_bd(bd)
{
}

template<class BOARD>
GoUctPlayoutPolicy<BOARD>::~GoUctPlayoutPolicy()
{
}

template<class BOARD>
inline const BOARD& GoUctPlayoutPolicy<BOARD>::Board() const
{
    return m_bd;
}

template<class BOARD>
void GoUctPlayoutPolicy<BOARD>::EndPlayout()
{
    // Default implementation does nothing
}

template<class BOARD>
void GoUctPlayoutPolicy<BOARD>::OnPlay()
{
    // Default implementation does nothing
}

template<class BOARD>
void GoUctPlayoutPolicy<BOARD>::StartPlayout()
{
    // Default implementation does nothing
}

#endif // 0

//----------------------------------------------------------------------------

#if 0

// TODO: GoUctPlayoutPolicyFactory was previously a base class, not it is a
// template parameter if GouctGlobalSearch. GoUctPlayoutPolicy should become
// a concept (Boost.Concept) that checks for the reuqired functions in the
// template parameter

/** Create instances of the playout policies.
    Safety information will be set later (before using) with SetSafe() and
    SetAllSafe(), because this class needs a multi-step construction
    in GoUctGlobalSearch.
*/
template<class BOARD>
class GoUctPlayoutPolicyFactory
{
public:
    GoUctPlayoutPolicyFactory();

    virtual ~GoUctPlayoutPolicyFactory();

    virtual GoUctPlayoutPolicy<BOARD>* Create(const BOARD& bd) = 0;
};


template<class BOARD>
GoUctPlayoutPolicyFactory<BOARD>::GoUctPlayoutPolicyFactory()
{
}

template<class BOARD>
GoUctPlayoutPolicyFactory<BOARD>::~GoUctPlayoutPolicyFactory()
{
}

#endif // 0

//----------------------------------------------------------------------------

/** Parameters for GoUctGlobalSearchState */
struct GoUctGlobalSearchStateParam
{
    /** Use the mercy rule.
        Count games early as win or loss, if stone difference on board
        exceeds a threshold of 30% of the total number of points on board.
    */
    bool m_mercyRule;

    /** Compute probabilities of territory in terminal positions. */
    bool m_territoryStatistics;

    /** Modify game result by score.
        This modifies the win/loss result (1/0) by the score of the end
        position. The modification is added for losses and subtracted for
        wins. The modification value is the score divided by the maximum
        score, which can be reached on the board, times the maximum
        score modification value.
        This helps to play moves to maximize the score even if the game is
        already clearly lost or won. Otherwise all moves look equal in
        clearly won or lost positions.
        It can also reduce the typical game length and could even have a
        positive effect on the playing strength.
        The modification can be disabled by setting the maximum score
        modification value to zero. The default value is 0.02.
    */
    float m_scoreModification;

    GoUctGlobalSearchStateParam();
};

//----------------------------------------------------------------------------

/** @page gouctpassmoves Handling of pass-moves in GoUctGlobalSearch

    @section gouctpassplayout Play-out phase

    Pass moves are not played in the play-out phase of the simulations as
    long as there are still moves for which GoUctUtil::GeneratePoint
    returns true, which are mainly moves that don't fill single point
    eyes (see GoUctUtil::GeneratePoint and GoBoardUtil::IsCompletelySurrounded
    for an exact definition). Therefore, it is a requirement on
    GoUctPlayoutPolicy::GenerateMove, not to return pass moves earlier.

    This requirement ensures that all simulated games terminate (as long as
    there is no super-ko cycle, because for speed reasons only simple ko's
    are checked) and that the terminal position after two passes in a row is
    a position that can be quickly evaluated with
    GoBoardUtil::ScoreSimpleEndPosition.

    @section gouctpassintree In-tree phase

    In the in-tree-phase of the game, pass moves are always allowed to avoid
    zugzwang situations, if there is a seki on the board. After two
    passes the game is terminated and scored with
    GoBoardUtil::TrompTaylorScore.
*/

/** Global UCT-Search for Go.
    - @ref gouctpassmoves
    @tparam POLICY The playout policy
*/
template<class POLICY>
class GoUctGlobalSearchState
    : public GoUctState
{
public:
    const SgBWSet& m_safe;

    const SgPointArray<bool>& m_allSafe;

    /** Probabilities that a point belongs to Black in a terminal position.
        Only computed if GoUctGlobalSearchStateParam::m_territoryStatistics.
    */
    SgPointArray<SgUctStatistics> m_territoryStatistics;

    /** Constructor.
        @param threadId The number of the thread. Needed for passing to
        constructor of SgUctThreadState.
        @param bd The board
        @param policy The random policy (takes ownership). It is possible to
        set the policy to null at construction time to allowed a multi-step
        construction; but then a policy has to be set with SetPolicy(), before
        the search is used.
        @param param Parameters. Stores a reference to the argument.
        @param safe Safety information. Stores a reference to the argument.
        @param allSafe Safety information. Stores a reference to the argument.
    */
    GoUctGlobalSearchState(std::size_t threadId, const GoBoard& bd,
                           POLICY* policy,
                           const GoUctGlobalSearchStateParam& param,
                           const SgBWSet& safe,
                           const SgPointArray<bool>& allSafe);

    ~GoUctGlobalSearchState();

    float Evaluate();

    void GenerateAllMoves(std::vector<SgMove>& moves);

    SgMove GeneratePlayoutMove(bool& skipRaveUpdate);

    void ExecutePlayout(SgMove move);

    void GameStart();

    void EndPlayout();

    void StartPlayout();

    void StartPlayouts();

    void StartSearch();

    POLICY* Policy();

    /** Set random policy.
        Sets a new random policy and deletes the old one, if it existed.
    */
    void SetPolicy(POLICY* policy);

    void ClearTerritoryStatistics();

private:
    const GoUctGlobalSearchStateParam& m_param;

    /** See SetMercyRule() */
    bool m_mercyRuleTriggered;

    /** Number of pass moves played in a row in the playout phase. */
    int m_passMovesPlayoutPhase;

    /** See SetMercyRule() */
    int m_mercyRuleThreshold;

    /** Difference of stones on board.
        Black counts positive.
    */
    int m_stoneDiff;

    /** Board move number at root node of search. */
    int m_initialMoveNumber;

    /** The area in which moves should be generated. */
    GoPointList m_area;

    /** See SetMercyRule() */
    float m_mercyRuleResult;

    /** Inverse of maximum score, one can reach on a board of the current
        size.
    */
    float m_invMaxScore;

    SgRandom m_random;

    std::auto_ptr<POLICY> m_policy;

    /** Not implemented */
    GoUctGlobalSearchState(const GoUctGlobalSearchState& search);

    /** Not implemented */
    GoUctGlobalSearchState& operator=(const GoUctGlobalSearchState& search);

    bool CheckMercyRule();

    template<class BOARD>
    float EvaluateBoard(const BOARD& bd, float komi);
};

template<class POLICY>
GoUctGlobalSearchState<POLICY>::GoUctGlobalSearchState(std::size_t threadId,
         const GoBoard& bd, POLICY* policy,
         const GoUctGlobalSearchStateParam& param,
         const SgBWSet& safe, const SgPointArray<bool>& allSafe)
    : GoUctState(threadId, bd),
      m_safe(safe),
      m_allSafe(allSafe),
      m_param(param),
      m_policy(policy)
{
    ClearTerritoryStatistics();
}

template<class POLICY>
GoUctGlobalSearchState<POLICY>::~GoUctGlobalSearchState()
{
}

/** See SetMercyRule() */
template<class POLICY>
bool GoUctGlobalSearchState<POLICY>::CheckMercyRule()
{
    SG_ASSERT(m_param.m_mercyRule);
    // Only used in playout; m_stoneDiff only defined in playout
    SG_ASSERT(IsInPlayout());
    if (m_stoneDiff >= m_mercyRuleThreshold)
    {
        m_mercyRuleTriggered = true;
        m_mercyRuleResult = (UctBoard().ToPlay() == SG_BLACK ? 1 : 0);
    }
    else if (m_stoneDiff <= -m_mercyRuleThreshold)
    {
        m_mercyRuleTriggered = true;
        m_mercyRuleResult = (UctBoard().ToPlay() == SG_WHITE ? 1 : 0);
    }
    else
        SG_ASSERT(! m_mercyRuleTriggered);
    return m_mercyRuleTriggered;
}

template<class POLICY>
void GoUctGlobalSearchState<POLICY>::ClearTerritoryStatistics()
{
    for (SgPointArray<SgUctStatistics>::NonConstIterator
             it(m_territoryStatistics); it; ++it)
        (*it).Clear();
}

template<class POLICY>
float GoUctGlobalSearchState<POLICY>::Evaluate()
{
    float komi = Board().Rules().Komi().ToFloat();
    if (IsInPlayout())
        return EvaluateBoard(UctBoard(), komi);
    else
        return EvaluateBoard(Board(), komi);
}

template<class POLICY>
template<class BOARD>
float GoUctGlobalSearchState<POLICY>::EvaluateBoard(const BOARD& bd,
                                                    float komi)
{
    float score;
    SgPointArray<SgEmptyBlackWhite> scoreBoard;
    SgPointArray<SgEmptyBlackWhite>* scoreBoardPtr;
    if (m_param.m_territoryStatistics)
        scoreBoardPtr = &scoreBoard;
    else
        scoreBoardPtr = 0;
    if (m_passMovesPlayoutPhase < 2)
        // Two passes not in playout phase, see comment in GenerateAllMoves()
        score = GoBoardUtil::TrompTaylorScore(bd, komi, scoreBoardPtr);
    else
    {
        if (m_param.m_mercyRule && m_mercyRuleTriggered)
            return m_mercyRuleResult;
        score = GoBoardUtil::ScoreEndPosition(bd, komi, m_safe,
                                              false, scoreBoardPtr);
    }
    if (m_param.m_territoryStatistics)
        for (typename BOARD::Iterator it(bd); it; ++it)
            switch (scoreBoard[*it])
            {
            case SG_BLACK:
                m_territoryStatistics[*it].Add(1);
                break;
            case SG_WHITE:
                m_territoryStatistics[*it].Add(0);
                break;
            case SG_EMPTY:
                m_territoryStatistics[*it].Add(0.5);
                break;
            }
    if (bd.ToPlay() != SG_BLACK)
        score *= -1;
    if (score > 0)
        return ((1 - m_param.m_scoreModification)
                + m_param.m_scoreModification * score * m_invMaxScore);
    else
        return (m_param.m_scoreModification
                + m_param.m_scoreModification * score * m_invMaxScore);
}

template<class POLICY>
void GoUctGlobalSearchState<POLICY>::ExecutePlayout(SgMove move)
{
    GoUctState::ExecutePlayout(move);
    const GoUctBoard& bd = UctBoard();
    if (bd.ToPlay() == SG_BLACK)
        m_stoneDiff -= bd.NuCapturedStones();
    else
        m_stoneDiff += bd.NuCapturedStones();
    m_policy->OnPlay();
}

template<class POLICY>
void GoUctGlobalSearchState<POLICY>::GenerateAllMoves(
                                                   std::vector<SgMove>& moves)
{
    SG_ASSERT(! IsInPlayout());
    const GoBoard& bd = Board();
    SG_ASSERT(! bd.Rules().AllowSuicide());

    if (GoBoardUtil::TwoPasses(bd))
        // Evaluate with Tromp-Taylor (we have no other evaluation that can
        // score arbitrary positions). However, if the rules don't require
        // CaptureDead(), the two passes need to be played in the search
        // sequence. This avoids cases, in which playing a pass (after the
        // opponent's last move in the real game was a pass) is only good
        // under Tromp-Taylor scoring (see
        // regression/sgf/pass/tromp-taylor-pass.sgf).
        // Both won't work in Japanese rules, but it is not easy to define
        // what a terminal position is in Japanese rules.
        if (bd.Rules().CaptureDead()
            || bd.MoveNumber() - m_initialMoveNumber >= 2)
            return;

    SgBlackWhite toPlay = bd.ToPlay();
    for (GoBoard::Iterator it(bd); it; ++it)
    {
        SgPoint p = *it;
        if (bd.IsEmpty(p)
            && ! GoEyeUtil::IsSimpleEye(bd, p, toPlay)
            && ! m_allSafe[p]
            && bd.IsLegal(p, toPlay))
            moves.push_back(p);
    }
    // Full randomization is too expensive and in most cases not necessary,
    // if prior knowledge is available for initialization or RAVE values are
    // available after playing the first move. However we put a random move
    // to the front, because the first move in a Go board iteration is often
    // a bad corner move
    if (moves.size() > 1)
        std::swap(moves[0], moves[m_random.Int(moves.size())]);
    moves.push_back(SG_PASS);
}

template<class POLICY>
SgMove GoUctGlobalSearchState<POLICY>::GeneratePlayoutMove(
                                                         bool& skipRaveUpdate)
{
    SG_ASSERT(IsInPlayout());
    if (m_param.m_mercyRule && CheckMercyRule())
        return SG_NULLMOVE;
    SgPoint move = m_policy->GenerateMove();
    SG_ASSERT(move != SG_NULLMOVE);
#ifndef NDEBUG
    // Check that policy generates pass only if no points are left for which
    // GeneratePoint() returns true. See GoUctPlayoutPolicy::GenerateMove()
    if (move == SG_PASS)
    {
        const GoUctBoard& bd = UctBoard();
        for (GoUctBoard::Iterator it(bd); it; ++it)
            SG_ASSERT(   bd.Occupied(*it)
                     || m_safe.OneContains(*it)
                     || GoBoardUtil::SelfAtari(bd, *it)
                     || ! GoUctUtil::GeneratePoint(bd, *it, bd.ToPlay()));
    }
    else
        SG_ASSERT(! m_safe.OneContains(move));
#endif
    // The position guaranteed to be a terminal position, which can be
    // evaluated with GoBoardUtil::ScoreSimpleEndPosition(), only after two
    // passes in a row, both of them generated by GeneratePlayoutMove() in
    // the playout phase
    if (move == SG_PASS)
    {
        skipRaveUpdate = true; // Don't update RAVE values for pass moves
        if (m_passMovesPlayoutPhase < 2)
            ++m_passMovesPlayoutPhase;
        else
            return SG_NULLMOVE;
    }
    else
        m_passMovesPlayoutPhase = 0;
    return move;
}

template<class POLICY>
void GoUctGlobalSearchState<POLICY>::GameStart()
{
    GoUctState::GameStart();
    int size = Board().Size();
    m_mercyRuleThreshold = static_cast<int>(0.3 * size * size);
}

template<class POLICY>
void GoUctGlobalSearchState<POLICY>::EndPlayout()
{
    GoUctState::EndPlayout();
    m_policy->EndPlayout();
}

template<class POLICY>
inline POLICY* GoUctGlobalSearchState<POLICY>::Policy()
{
    return m_policy.get();
}

template<class POLICY>
void GoUctGlobalSearchState<POLICY>::SetPolicy(POLICY* policy)
{
    m_policy.reset(policy);
}

template<class POLICY>
void GoUctGlobalSearchState<POLICY>::StartPlayout()
{
    GoUctState::StartPlayout();
    m_passMovesPlayoutPhase = 0;
    m_mercyRuleTriggered = false;
    const GoBoard& bd = Board();
    m_stoneDiff = bd.All(SG_BLACK).Size() - bd.All(SG_WHITE).Size();
    m_policy->StartPlayout();
}

template<class POLICY>
void GoUctGlobalSearchState<POLICY>::StartPlayouts()
{
    GoUctState::StartPlayouts();
}

template<class POLICY>
void GoUctGlobalSearchState<POLICY>::StartSearch()
{
    GoUctState::StartSearch();
    const GoBoard& bd = Board();
    int size = bd.Size();
    float maxScore = size * size + bd.Rules().Komi().ToFloat();
    m_invMaxScore = 1 / maxScore;
    m_initialMoveNumber = bd.MoveNumber();
    ClearTerritoryStatistics();
}

//----------------------------------------------------------------------------

/** Factory for creating a GoUctGlobalSearchState.
    @tparam POLICY The playout policy
    @tparam FACTORY The factory for the playout policy
*/
template<class POLICY, class FACTORY>
class GoUctGlobalSearchStateFactory
    : public SgUctThreadStateFactory
{
public:
    /** Constructor.
        @param bd
        @param playoutPolicyFactory Factory for playout policies.
        Stores a reference. Lifetime of parameter must exceed the lifetime of
        this instance.
        @param safe
        @param allSafe
    */
    GoUctGlobalSearchStateFactory(GoBoard& bd,
                                  FACTORY& playoutPolicyFactory,
                                  const SgBWSet& safe,
                                  const SgPointArray<bool>& allSafe);

    SgUctThreadState* Create(std::size_t threadId, const SgUctSearch& search);

private:
    GoBoard& m_bd;

    FACTORY& m_playoutPolicyFactory;

    const SgBWSet& m_safe;

    const SgPointArray<bool>& m_allSafe;
};

template<class POLICY, class FACTORY>
GoUctGlobalSearchStateFactory<POLICY,FACTORY>
::GoUctGlobalSearchStateFactory(GoBoard& bd,
                  FACTORY& playoutPolicyFactory,
                  const SgBWSet& safe,
                  const SgPointArray<bool>& allSafe)
    : m_bd(bd),
      m_playoutPolicyFactory(playoutPolicyFactory),
      m_safe(safe),
      m_allSafe(allSafe)
{
}

//----------------------------------------------------------------------------

/** Full board Monte-Carlo tree search.
    @tparam POLICY The playout policy
    @tparam FACTORY The factory for the playout policy
*/
template<class POLICY, class FACTORY>
class GoUctGlobalSearch
    : public GoUctSearch
{
public:
    GoUctGlobalSearchStateParam m_param;

    /** Constructor.
        @param bd
        @param playoutPolicyFactory Creates multiple instances of the playout
        policies. Takes ownership. playoutPolicyFactory should not have
        SetSafe() and SetAllSafe() already set, because the search will call
        these functions using its own safety information.
    */
    GoUctGlobalSearch(GoBoard& bd,
                      FACTORY* playoutPolicyFactory);

    /** @name Pure virtual functions of SgUctSearch */
    // @{

    float UnknownEval() const;

    // @} // @name


    /** @name Virtual functions of SgUctSearch */
    // @{

    void OnStartSearch();

    void OnSearchIteration(std::size_t gameNumber, int threadId,
                           const SgUctGameInfo& info);

    // @} // @name


    /** Set default search parameters optimized for a board size. */
    void SetDefaultParameters(int boardSize);

    /** Output live graphics commands for GoGui.
        Similar to the GOUCT_LIVEGFX_COUNTS mode in GoUctSearch, but the
        influence data shows the terriroy statistics (which must be enabled)
        instead of the move values. No move counts are shown.
        The live graphics interval from GoUctSearch will be used.
        @see GoUctSearch::LiveGfxInterval()
    */
    bool GlobalSearchLiveGfx() const;

    /** See GlobalSearchLiveGfx() */
    void SetGlobalSearchLiveGfx(bool enable);

private:
    SgBWSet m_safe;

    SgPointArray<bool> m_allSafe;

    std::auto_ptr<FACTORY> m_playoutPolicyFactory;

    GoRegionBoard m_regions;

    /** See GlobalSearchLiveGfx() */
    bool m_globalSearchLiveGfx;
};

template<class POLICY, class FACTORY>
GoUctGlobalSearch<POLICY,FACTORY>::GoUctGlobalSearch(GoBoard& bd,
                                                     FACTORY* playoutFactory)
    : GoUctSearch(bd, 0),
      m_regions(bd),
      m_globalSearchLiveGfx(GOUCT_LIVEGFX_NONE)
{
    SgUctThreadStateFactory* stateFactory =
        new GoUctGlobalSearchStateFactory<POLICY,FACTORY>(bd,
                                                          *playoutFactory,
                                                          m_safe, m_allSafe);
    SetThreadStateFactory(stateFactory);
    SetDefaultParameters(bd.Size());
}

template<class POLICY, class FACTORY>
inline bool GoUctGlobalSearch<POLICY,FACTORY>::GlobalSearchLiveGfx() const
{
    return m_globalSearchLiveGfx;
}

template<class POLICY, class FACTORY>
void GoUctGlobalSearch<POLICY,FACTORY>::OnSearchIteration(
                                          std::size_t gameNumber,
                                          int threadId,
                                          const SgUctGameInfo& info)
{
    GoUctSearch::OnSearchIteration(gameNumber, threadId, info);
    if (m_globalSearchLiveGfx && threadId == 0
        && gameNumber % LiveGfxInterval() == 0)
    {
        const GoUctGlobalSearchState<POLICY>& state =
            dynamic_cast<GoUctGlobalSearchState<POLICY>&>(ThreadState(0));
        SgDebug() << "gogui-gfx:\n";
        GoUctUtil::GfxBestMove(*this, ToPlay(), SgDebug());
        GoUctUtil::GfxTerritoryStatistics(state.m_territoryStatistics,
                                          Board(), SgDebug());
        GoUctUtil::GfxStatus(*this, SgDebug());
        SgDebug() << '\n';
    }
}

template<class POLICY, class FACTORY>
void GoUctGlobalSearch<POLICY,FACTORY>::OnStartSearch()
{
    GoUctSearch::OnStartSearch();
    m_safe.Clear();
    m_allSafe.Fill(false);
    if (GOUCT_USE_SAFETY_SOLVER)
    {
        GoBoard& bd = Board();
        GoSafetySolver solver(bd, &m_regions);
        solver.FindSafePoints(&m_safe);
        for (GoBoard::Iterator it(bd); it; ++it)
            m_allSafe[*it] = m_safe.OneContains(*it);
    }
    if (m_globalSearchLiveGfx && ! m_param.m_territoryStatistics)
        SgWarning() <<
            "GoUctGlobalSearch: "
            "live graphics need territory statistics enabled\n";
}

template<class POLICY, class FACTORY>
void GoUctGlobalSearch<POLICY,FACTORY>::SetDefaultParameters(int boardSize)
{
    SetFirstPlayUrgency(1);
    SetMoveSelect(SG_UCTMOVESELECT_COUNT);
    SetRave(true);
    SetRaveWeightInitial(1.0);
    SetRaveWeightFinal(5000);
    SetExpandThreshold(2);
    if (boardSize <= 13)
    {
        // These parameters were mainly tested on 9x9
        SetNoBiasTerm(false);
        SetBiasTermConstant(0.02);
    }
    else
    {
        // These parameters were mainly tested on 19x19
        SetNoBiasTerm(true);
    }
}

template<class POLICY, class FACTORY>
inline void GoUctGlobalSearch<POLICY,FACTORY>::SetGlobalSearchLiveGfx(
                                                                  bool enable)
{
    m_globalSearchLiveGfx = enable;
}

template<class POLICY, class FACTORY>
float GoUctGlobalSearch<POLICY,FACTORY>::UnknownEval() const
{
    // Note: 0.5 is not a possible value for a Bernoulli variable, better
    // use 0?
    return 0.5;
}

//----------------------------------------------------------------------------

template<class POLICY, class FACTORY>
SgUctThreadState* GoUctGlobalSearchStateFactory<POLICY,FACTORY>::Create(
                              std::size_t threadId, const SgUctSearch& search)
{
    const GoUctGlobalSearch<POLICY,FACTORY>& globalSearch =
        dynamic_cast<const GoUctGlobalSearch<POLICY,FACTORY>&>(search);
    GoUctGlobalSearchState<POLICY>* state =
        new GoUctGlobalSearchState<POLICY>(threadId, globalSearch.Board(), 0,
                                   globalSearch.m_param, m_safe, m_allSafe);
    POLICY* policy = m_playoutPolicyFactory.Create(state->UctBoard());
    state->SetPolicy(policy);
    return state;
}

//----------------------------------------------------------------------------

#endif // GOUCT_GLOBALSEARCH_H
