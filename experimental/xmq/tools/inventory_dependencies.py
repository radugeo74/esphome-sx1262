#!/usr/bin/env python3
"""Inventory dependency/API surface needed for XMQ driver and IXML semantics.
Diagnostic only: no parser replacement and no claims of embedded compatibility.
"""
import argparse
import json
import re
from pathlib import Path

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--src",type=Path,required=True,help="Pinned wmbusmeters/src directory")
    ap.add_argument("--out",type=Path,required=True)
    args=ap.parse_args()
    names=["xmq.c","xmq.h","xmq_internals.h","driver_dynamic.cc","drivers.cc"]
    report={"files":{},"libxml_directives":[],"xmq_api_calls":{}}
    for name in names:
        p=args.src/name
        if not p.is_file():
            report["files"][name]={"missing":True}
            continue
        source=p.read_text(encoding="utf-8",errors="replace")
        includes=re.findall(r'(?m)^\s*#\s*include\s*[<"]([^>"]+)[>"]',source)
        report["files"][name]={"bytes":p.stat().st_size,"includes":sorted(set(includes))}
        if name=="xmq.c":
            lines=source.splitlines()
            for i,line in enumerate(lines,1):
                if re.search(r'(?:#\s*(?:include|if|ifdef|ifndef|elif).*libxml|XMQ_NO_LIBXML|\bxml(?:Doc|Node|XPath|Free|Read|Parse|New|Get))',line):
                    report["libxml_directives"].append({"line":i,"code":line.strip()[:180]})
        if name=="driver_dynamic.cc":
            matches=re.findall(r'\b(xmq[A-Z][A-Za-z0-9_]*)\s*\(',source)
            report["xmq_api_calls"]=dict(sorted((k,matches.count(k)) for k in set(matches)))
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print("Dependency inventory:")
    for name,data in report["files"].items():
        print(name,data.get("bytes","MISSING"),"bytes; direct includes:",",".join(data.get("includes",[]))[:220])
    print("XMQ API used by driver_dynamic.cc:",report["xmq_api_calls"])
    print("libxml/conditional references:",len(report["libxml_directives"]))
    print("JSON report:",args.out)

if __name__=="__main__":
    main()
