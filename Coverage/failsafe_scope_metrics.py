import json,pathlib,argparse
root=pathlib.Path('/home/hamzaahmad/PX4-Autopilot')
def metrics(path):
 lines={}; branches=[]; functions={}
 for record in path.read_text().splitlines():
  if record.startswith('DA:'):
   ln,hits=record[3:].split(',')[:2];lines[int(ln)]=int(hits)
  elif record.startswith('BRDA:'):
   ln,block,branch,taken=record[5:].split(',');branches.append((int(ln),block,branch,taken))
  elif record.startswith('FNDA:'):
   hits,name=record[5:].split(',',1);functions[name]=int(hits)
 def scope(low,high):
  ls={k:v for k,v in lines.items() if low<=k<=high}
  bs=[b for b in branches if low<=b[0]<=high]; numeric=[b for b in bs if b[3]!='-']
  return dict(lines_total=len(ls),lines_hit=sum(v>0 for v in ls.values()),line_percent=100*sum(v>0 for v in ls.values())/len(ls) if ls else None,branches_total=len(bs),branches_numeric=len(numeric),branches_dash=len(bs)-len(numeric),branches_hit=sum(int(b[3])>0 for b in numeric),branch_percent=100*sum(int(b[3])>0 for b in numeric)/len(bs) if bs else None,numeric_only_branch_percent=100*sum(int(b[3])>0 for b in numeric)/len(numeric) if numeric else None,zero_lines=[k for k,v in ls.items() if not v],zero_branches=[b for b in bs if b[3]=='-' or int(b[3])==0])
 return dict(whole=scope(0,100000),selected=scope(445,642),functions_total=len(functions),functions_hit=sum(v>0 for v in functions.values()))
if __name__ == "__main__":
 parser=argparse.ArgumentParser(description="Count DA and BRDA records, including dash branches in the denominator.")
 parser.add_argument("--input",type=pathlib.Path)
 parser.add_argument("--output",type=pathlib.Path)
 args=parser.parse_args()
 if args.input:
  data={"input":str(args.input.resolve()),**metrics(args.input)}
  if args.output: args.output.write_text(json.dumps(data,indent=2)+"\n")
 else:
  data={}
  for label,name in [("before","failsafe-production-student-only.info"),("after","failsafe-production-student-final.info")]:
   path=root/"coverage"/name
   if path.exists(): data[label]=metrics(path)
  (args.output or root/"coverage"/"failsafe-selected-scope-final.json").write_text(json.dumps(data,indent=2)+"\n")
 print(json.dumps(data,indent=2))
