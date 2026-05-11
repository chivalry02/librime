//
// Copyright RIME Developers
// Distributed under the BSD License
//
// 2011-06-20 GONG Chen <chen.sst@gmail.com>
//

#include <rime/candidate.h>
#include <rime/common.h>
#include <rime/segmentation.h>
#include <rime/translation.h>
#include <rime/gear/echo_translator.h>

namespace rime {

class EchoTranslation : public UniqueTranslation {
 public:
  EchoTranslation(const an<Candidate>& candidate)
      : UniqueTranslation(candidate) {}
  virtual int Compare(an<Translation> other, const CandidateList& candidates) {
    if (!candidates.empty() || (other && !other->exhausted())) {
      set_exhausted(true);
    }
    return UniqueTranslation::Compare(other, candidates);
  }
};

EchoTranslator::EchoTranslator(const Ticket& ticket) : Translator(ticket) {}

// Translator（基类）在引擎中的职责：对某个 Segment 对应的输入子串做一次“查询”，
// 返回 Translation（可迭代的一组候选）；引擎会合并多位 Translator 的 Translation，
// 再经 Menu/Filter 等形成最终候选列表。Query 是唯一必须实现的接口。
//
// EchoTranslator：不做词典或语法翻译，只是把当前段的原始输入“原样回显”为一条候选，
// 常用于无匹配时仍可看到拼音/编码，或作为占位；真实词库候选通常优先级更高。
an<Translation> EchoTranslator::Query(const string& input,
                                      const Segment& segment) {
  DLOG(INFO) << "input = '" << input << "', [" << segment.start << ", "
             << segment.end << ")";
  // 空输入不产生任何 Translation，表示本 Translator 对此段不参与贡献。
  if (input.empty()) {
    return nullptr;
  }
  // SimpleCandidate 类型设为 "raw"：表示这是未经正规翻译管道的原始串。
  // start/end 与 segment 对齐，便于与 composition 其它段衔接。
  auto candidate =
      New<SimpleCandidate>("raw", segment.start, segment.end, input);
  if (candidate) {
    // quality 极低：合并候选时通常会排在词典/脚本翻译结果之后；
    // 若仅有 echo，用户仍能看到与输入一致的“影子”候选。
    candidate->set_quality(-100);  // lowest priority
  }
  // EchoTranslation（见上方）：在与其它 Translation 同台比较时，若已有竞品候选或
  // 竞品 Translation 仍未耗尽，会主动 exhausted，避免在正常有词时与用户抢焦点。
  return New<EchoTranslation>(candidate);
}

}  // namespace rime
