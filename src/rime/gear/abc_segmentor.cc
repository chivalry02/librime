//
// Copyright RIME Developers
// Distributed under the BSD License
//
// 2011-05-15 GONG Chen <chen.sst@gmail.com>
//
#include <rime/common.h>
#include <rime/config.h>
#include <rime/schema.h>
#include <rime/segmentation.h>
#include <rime/gear/abc_segmentor.h>

static const char kRimeAlphabet[] = "zyxwvutsrqponmlkjihgfedcba";

namespace rime {

AbcSegmentor::AbcSegmentor(const Ticket& ticket)
    : Segmentor(ticket), alphabet_(kRimeAlphabet) {
  if (!ticket.schema)
    return;
  if (Config* config = ticket.schema->config()) {
    config->GetString("speller/alphabet", &alphabet_);
    config->GetString("speller/delimiter", &delimiter_);
    config->GetString("speller/initials", &initials_);
    config->GetString("speller/finals", &finals_);
    if (auto extra_tags = config->GetList("abc_segmentor/extra_tags")) {
      for (size_t i = 0; i < extra_tags->size(); ++i) {
        if (auto value = As<ConfigValue>(extra_tags->GetAt(i))) {
          extra_tags_.insert(value->str());
        }
      }
    }
  }
  if (initials_.empty()) {
    initials_ = alphabet_;
  }
}

bool AbcSegmentor::Proceed(Segmentation* segmentation) {
  // Segmentor 的职责：在输入串上识别“可翻译区间”，并把区间以 Segment 形式交给后续 Translator。
  // AbcSegmentor 专门识别字母拼写片段（通常是拼音/形码编码），不负责产出候选词本身。
  const string& input(segmentation->input());
  DLOG(INFO) << "abc_segmentor: " << input;
  // j: 当前轮次允许分段的起点（由 Segmentation 状态决定）
  // k: 从 j 向右扫描，寻找本次可接受的最长合法区间 [j, k)
  size_t j = segmentation->GetCurrentStartPosition();
  size_t k = j;
  // 语法约束状态：当前位置是否“期待一个声母位字符”。
  // 初始为 true，意味着片段开头通常应是声母（或允许分隔符）。
  bool expecting_an_initial = true;
  for (; k < input.length(); ++k) {
    // 仅接受两类字符：字母表字符、分隔符字符。
    bool is_letter = alphabet_.find(input[k]) != string::npos;
    // k != 0: 避免把首字符当分隔符（片段不能以分隔符开头）。
    bool is_delimiter = (k != 0) && (delimiter_.find(input[k]) != string::npos);
    if (!is_letter && !is_delimiter)
      break;
    // 当前字符在声母/韵母集合中的归类，用于更新状态机。
    bool is_initial = initials_.find(input[k]) != string::npos;
    bool is_final = finals_.find(input[k]) != string::npos;
    // 若当前位置要求声母，但既不是声母也不是分隔符，则判定为非法拼写并截断。
    if (expecting_an_initial && !is_initial && !is_delimiter) {
      break;  // not a valid spelling.
    }
    // 更新“下一字符”的期待状态：
    // - 当前是韵母 or 分隔符：下一位通常应回到声母位；
    // - 否则保持 false，允许继续处于韵母位延展。
    expecting_an_initial = is_final || is_delimiter;
  }
  DLOG(INFO) << "[" << j << ", " << k << ")";
  if (j < k) {
    // 识别到合法区间后，生成一个 Segment 并打上 abc 标签；
    // 后续 Translator 可依据标签决定是否参与该段翻译。
    Segment segment(j, k);
    segment.tags.insert("abc");
    // 支持通过配置注入额外标签，便于 schema 做更细粒度路由。
    for (const string& tag : extra_tags_) {
      segment.tags.insert(tag);
    }
    segmentation->AddSegment(segment);
  }
  // 返回 true 表示本 Segmentor 不阻断分段轮次，允许其他 Segmentor 继续尝试补充分段结果。
  return true;
}

}  // namespace rime
