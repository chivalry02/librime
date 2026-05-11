//
// Copyright RIME Developers
// Distributed under the BSD License
//
// 2011-12-12 GONG Chen <chen.sst@gmail.com>
//
// -----------------------------------------------------------------------------
// 管线中的 Filter（过滤器）
//
// Filter 位于 Translator 产出 Translation 之后：引擎会把「候选迭代器」和当前轮已积累的
// CandidateList* 交给各个 Filter::Apply。Filter 可以：
// - 包装一层 Translation（惰性改写迭代逻辑），或
// - 读写 candidates（例如与其它翻译路径合并结果）。
//
// Uniquifier 是典型的「包装 Translation」：不改变词典语义，只按「词条正文 text()」
// 合并重复项，避免菜单里出现多条完全相同的汉字串。
// -----------------------------------------------------------------------------
#include <rime/candidate.h>
#include <rime/common.h>
#include <rime/translation.h>
#include <rime/gear/uniquifier.h>

namespace rime {

// 包装底层 Translation，在每次 Prepare 出下一个候选时做去重。
// 继承 CacheTranslation：通常会缓冲/缓存底层迭代状态；此处额外持有 candidates_
// 指针，用于与「此前已在列表里的候选」按 text() 比对。
class UniquifiedTranslation : public CacheTranslation {
 public:
  UniquifiedTranslation(an<Translation> translation, CandidateList* candidates)
      : CacheTranslation(translation), candidates_(candidates) {
    // 构造后即收紧一轮：若栈顶已有重复 text，会合并或跳过直到遇到唯一项或耗尽。
    Uniquify();
  }
  // 每前进一条底层候选后再次调用 Uniquify()，保证对外暴露的序列始终「按 text 去重」。
  virtual bool Next();

 protected:
  // 从当前 Peek() 开始向后跳过重复：重复则合并进 UniquifiedCandidate 并底层 Next()，
  // 直到遇到第一条尚未出现在 candidates_ 中的 text，或 Translation 耗尽。
  bool Uniquify();

  an<Translation> translation_;
  CandidateList* candidates_;
};

bool UniquifiedTranslation::Next() {
  // 先沿底层 Translation 走一步，再对新栈顶做一轮 uniquify。
  return CacheTranslation::Next() && Uniquify();
}

// 在 [begin, end) 中查找第一条与 target->text() 相同的候选（含此前路径已插入列表的项）。
static CandidateList::iterator find_text_match(const an<Candidate>& target,
                                               CandidateList::iterator begin,
                                               CandidateList::iterator end) {
  for (auto iter = begin; iter != end; ++iter) {
    if ((*iter)->text() == target->text()) {
      return iter;
    }
  }
  return end;
}

bool UniquifiedTranslation::Uniquify() {
  while (!exhausted()) {
    auto next = Peek();
    CandidateList::iterator previous =
        find_text_match(next, candidates_->begin(), candidates_->end());
    if (previous == candidates_->end()) {
      // 正文尚未出现在候选列表中：本条可作为独立项交给上层，本轮 uniquify 结束。
      return true;
    }
    // 与列表中已有条目正文重复：不单独占用一行，合并到「复合候选」UniquifiedCandidate。
    auto uniquified = As<UniquifiedCandidate>(*previous);
    if (!uniquified) {
      *previous = uniquified =
          New<UniquifiedCandidate>(*previous, "uniquified");
    }
    uniquified->Append(next);
    // 丢弃当前重复的 Peek，继续看下一条底层候选是否仍重复。
    CacheTranslation::Next();
  }
  return false;
}

// ---------------------------------------------------------------------------
// Uniquifier（对外注册的 Filter 组件）
//
// Apply 收到上层传来的 Translation 与本轮 CandidateList*：
// 返回 UniquifiedTranslation，在迭代过程中维护「按 text 合并」的不变量。
// （Filter::AppliesToSegment 在基类默认为 true；schema 里可对特定 segment 关闭。）
// ---------------------------------------------------------------------------

Uniquifier::Uniquifier(const Ticket& ticket) : Filter(ticket) {}

an<Translation> Uniquifier::Apply(an<Translation> translation,
                                  CandidateList* candidates) {
  return New<UniquifiedTranslation>(translation, candidates);
}

}  // namespace rime
