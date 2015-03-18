#ifndef EXELL_XLSXWORKBOOK_
#define EXELL_XLSXWORKBOOK_

#include <Rcpp.h>
#include "rapidxml.h"
#include "CellType.h"

// Simple parser: does not check that order of numbers and letters is correct
std::pair<int, int> parseRef(std::string ref) {
  int col = 0, row = 0;

  for (std::string::iterator cur = ref.begin(); cur != ref.end(); ++cur) {
    if (*cur >= '0' && *cur <= '9') {
      row = row * 10 + (*cur - '0');
    } else if (*cur >= 'A' && *cur <= 'Z') {
      col = 26 * col + (*cur - 'A' + 1);
    } else {
      Rcpp::stop("Invalid character ('%s') in cell ref", *cur);
    }
  }

  return std::make_pair(row, col);
}

inline std::string zip_buffer(std::string zip_path, std::string file_path) {
  Rcpp::Environment exellEnv = Rcpp::Environment("package:exell");
  Rcpp::Function zip_buffer = exellEnv["zip_buffer"];

  RawVector xml = Rcpp::as<RawVector>(zip_buffer(zip_path, file_path));
  std::string buffer(RAW(xml), RAW(xml) + xml.size());
  buffer.push_back('\0');

  return buffer;
}

class XlsxWorkBook {
  std::string path_;
  std::set<int> dateStyles_;


public:

  XlsxWorkBook(std::string path): path_(path) {
    cacheDateStyles();
  }

  std::vector<std::string> sheets() {
    std::string workbookXml = zip_buffer(path_, "xl/workbook.xml");
    rapidxml::xml_document<> workbook;
    workbook.parse<0>(&workbookXml[0]);

    std::vector<std::string> sheetNames;

    rapidxml::xml_node<>* root = workbook.first_node("workbook");
    if (root == NULL)
      return sheetNames;

    rapidxml::xml_node<>* sheets = root->first_node("sheets");
    if (sheets == NULL)
      return sheetNames;

    for (rapidxml::xml_node<>* sheet = sheets->first_node();
         sheet; sheet = sheet->next_sibling()) {
      std::string value(sheet->first_attribute("name")->value());
      sheetNames.push_back(value);
    }

    return sheetNames;
  }

  std::vector<std::string> strings() {
    std::string sharedStringsXml = zip_buffer(path_, "xl/sharedStrings.xml");
    rapidxml::xml_document<> sharedStrings;
    sharedStrings.parse<0>(&sharedStringsXml[0]);

    std::vector<std::string> strings;

    rapidxml::xml_node<>* sst = sharedStrings.first_node("sst");
    if (sst == NULL)
      return strings;

    rapidxml::xml_attribute<>* count = sst->first_attribute("count");
    if (count != NULL) {
      int n = atoi(count->value());
      strings.reserve(n);
    }

    for (rapidxml::xml_node<>* string = sst->first_node();
         string; string = string->next_sibling()) {
      std::string value(string->first_node("t")->value());
      strings.push_back(value);
    }

    return strings;
  }

  std::set<int> dateStyles() {
    return dateStyles_;
  }

private:

  void cacheDateStyles() {
    std::string sharedStringsXml = zip_buffer(path_, "xl/styles.xml");
    rapidxml::xml_document<> sharedStrings;
    sharedStrings.parse<0>(&sharedStringsXml[0]);

    rapidxml::xml_node<>* styleSheet = sharedStrings.first_node("styleSheet");
    if (styleSheet == NULL)
      return;

    // Figure out which custom formats are dates
    std::set<int> customDateFormats;
    rapidxml::xml_node<>* numFmts = styleSheet->first_node("numFmts");
    if (numFmts != NULL) {
      for (rapidxml::xml_node<>* numFmt = numFmts->first_node();
           numFmt; numFmt = numFmt->next_sibling()) {
        std::string code(numFmt->first_attribute("formatCode")->value());
        int id = atoi(numFmt->first_attribute("numFmtId")->value());

        if (isDateFormat(code))
          customDateFormats.insert(id);
      }
    }

    // Cache styles that have date formatting
    rapidxml::xml_node<>* cellXfs = styleSheet->first_node("cellXfs");
    if (cellXfs == NULL)
      return;

    int i = 0;
    for (rapidxml::xml_node<>* cellXf = cellXfs->first_node();
         cellXf; cellXf = cellXf->next_sibling()) {
      int formatId = atoi(cellXf->first_attribute("numFmtId")->value());
      if (isDateTime(formatId, customDateFormats))
        dateStyles_.insert(i);
      ++i;
    }

  }

  std::set<int> customDateFormats() const {
    std::string sharedStringsXml = zip_buffer(path_, "xl/sharedStrings.xml");
  }

};

#endif