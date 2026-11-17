#pragma once
// Experimental XMQ calendar subset: ISO date + integer months + integer hours.
// Parses expressions from source XMQ, not driver names; fails closed on other
// calendar operations. Pure C++17, independent of ESPHome and operating-system
// timezone. Exact calendar-day arithmetic, not 30-day approximations.
#include "xmq_expression.h"
#include <cstdint>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace xmq_runtime {

struct CalendarError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

inline std::string calendar_trim(const std::string &s) {
  const auto a=s.find_first_not_of(" \n\r\t");
  if (a==std::string::npos) return "";
  return s.substr(a,s.find_last_not_of(" \n\r\t")-a+1);
}

inline bool wrapped(const std::string &s) {
  if (s.size()<2 || s.front()!='(' || s.back()!=')') return false;
  int depth=0;
  for (size_t i=0;i<s.size();i++) {
    if (s[i]=='(') ++depth;
    else if (s[i]==')') {
      if (--depth==0 && i+1!=s.size()) return false;
      if (depth<0) return false;
    }
  }
  return depth==0;
}
inline std::string calendar_unwrap(std::string s) {
  s=calendar_trim(s);
  while (wrapped(s)) s=calendar_trim(s.substr(1,s.size()-2));
  return s;
}

inline std::vector<std::string> split_calendar_add(const std::string &s) {
  std::vector<std::string> out;
  int depth=0;
  char quote=0;
  size_t start=0;
  for (size_t i=0;i<s.size();i++) {
    const char c=s[i];
    if (quote) { if (c==quote) quote=0; continue; }
    if (c=='\'' || c=='"') { quote=c; continue; }
    if (c=='(') ++depth;
    else if (c==')') {
      if (--depth<0) throw CalendarError("unbalanced parentheses");
    } else if (c=='+' && depth==0) {
      out.push_back(calendar_trim(s.substr(start,i-start)));
      start=i+1;
    }
  }
  if (quote || depth) throw CalendarError("unbalanced calendar expression");
  out.push_back(calendar_trim(s.substr(start)));
  return out;
}

inline int64_t calendar_integer(const Number &n) {
  if (n.dimension!=Dimension::Counter || !std::isfinite(n.value) ||
      std::trunc(n.value)!=n.value || n.value < -1000000 ||
      n.value > 1000000)
    throw CalendarError("calendar offset requires bounded integral counter");
  return static_cast<int64_t>(n.value);
}

inline int64_t floor_div(int64_t a,int64_t b) {
  if (b<=0) throw CalendarError("invalid calendar divisor");
  int64_t q=a/b, r=a%b;
  if (r<0) --q;
  return q;
}
inline bool leap(int64_t y) {
  return y%4==0 && (y%100!=0 || y%400==0);
}
inline int days_in_month(int64_t y,unsigned m) {
  static constexpr int dim[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
  if (m<1 || m>12) throw CalendarError("invalid month");
  return dim[m]+(m==2 && leap(y)?1:0);
}

// Gregorian civil <-> days since 1970-01-01, using 400-year eras.
inline int64_t civil_days(int64_t y,unsigned m,unsigned d) {
  if (y<1600 || y>3000 || d<1 ||
      d>static_cast<unsigned>(days_in_month(y,m)))
    throw CalendarError("date outside supported range");
  y -= m<=2;
  const int64_t era=floor_div(y,400);
  const unsigned yoe=static_cast<unsigned>(y-era*400);
  const unsigned mp=m>2?m-3:m+9;
  const unsigned doy=(153*mp+2)/5+d-1;
  const unsigned doe=yoe*365+yoe/4-yoe/100+doy;
  return era*146097+doe-719468;
}
inline void days_civil(int64_t days,int64_t &y,unsigned &m,unsigned &d) {
  days+=719468;
  const int64_t era=floor_div(days,146097);
  const unsigned doe=static_cast<unsigned>(days-era*146097);
  const unsigned yoe=(doe-doe/1460+doe/36524-doe/146096)/365;
  y=static_cast<int64_t>(yoe)+era*400;
  const unsigned doy=doe-(365*yoe+yoe/4-yoe/100);
  const unsigned mp=(5*doy+2)/153;
  d=doy-(153*mp+2)/5+1;
  m=mp<10?mp+3:mp-9;
  y+=m<=2;
}

inline unsigned iso_part(const std::string &s,size_t start,size_t digits) {
  if (start+digits>s.size()) throw CalendarError("incomplete ISO date");
  unsigned result=0;
  for (size_t i=start;i<start+digits;i++) {
    if (s[i]<'0'||s[i]>'9') throw CalendarError("invalid ISO date");
    result=result*10+static_cast<unsigned>(s[i]-'0');
  }
  return result;
}

class CalendarExpression {
 public:
  explicit CalendarExpression(const std::map<std::string,Number> &variables)
    : variables_(variables) {}

  std::string evaluate(const std::string &expression) const {
    if (expression.size()>4096) throw CalendarError("calendar source size limit");
    const auto parts=split_calendar_add(calendar_unwrap(expression));
    if (parts.empty() || parts.size()>16) throw CalendarError("calendar term count");
    auto origin=calendar_unwrap(parts[0]);
    if (origin.size()!=12 || origin[0]!='\'' || origin[11]!='\'' ||
        origin[5]!='-' || origin[8]!='-')
      throw CalendarError("ISO base date required");
    const int64_t base_year=iso_part(origin,1,4);
    const unsigned base_month=iso_part(origin,6,2);
    const unsigned base_day=iso_part(origin,9,2);
    (void)civil_days(base_year,base_month,base_day);
    int64_t months=0, hours=0;
    for (size_t i=1;i<parts.size();i++) {
      const auto term=calendar_unwrap(parts[i]);
      int depth=0;
      size_t multiply=std::string::npos;
      for (size_t j=0;j<term.size();j++) {
        if (term[j]=='(') ++depth;
        else if (term[j]==')') --depth;
        else if (term[j]=='*' && depth==0) multiply=j;
        if (depth<0) throw CalendarError("unbalanced calendar term");
      }
      if (depth || multiply==std::string::npos)
        throw CalendarError("unsupported calendar offset");
      const auto scalar=calendar_unwrap(term.substr(0,multiply));
      const auto unit=calendar_trim(term.substr(multiply+1));
      int64_t n=0;
      try { n=calendar_integer(NumericExpression(variables_).evaluate(scalar)); }
      catch (const ExpressionError &e) { throw CalendarError(e.what()); }
      if (unit=="1month") months+=n;
      else if (unit=="24h") hours+=n*24;
      else throw CalendarError("unsupported calendar offset unit");
      if (months>100000 || months< -100000 ||
          hours>1000000 || hours< -1000000)
        throw CalendarError("calendar offset too large");
    }
    const int64_t month_index=base_year*12+static_cast<int64_t>(base_month)-1+months;
    const int64_t target_year=floor_div(month_index,12);
    const unsigned target_month=static_cast<unsigned>(month_index-target_year*12+1);
    // This subset uses strictly valid dates; no month-end clamping.
    const int64_t day=civil_days(target_year,target_month,base_day)
                     +floor_div(hours,24);
    int64_t y=0;
    unsigned m=0,d=0;
    days_civil(day,y,m,d);
    if (y<1600 || y>3000) throw CalendarError("output date out of range");
    std::ostringstream out;
    out<<std::setfill('0')<<std::setw(4)<<y<<"-"<<std::setw(2)<<m
       <<"-"<<std::setw(2)<<d;
    return out.str();
  }

 private:
  const std::map<std::string,Number> &variables_;
};

}  // namespace xmq_runtime
