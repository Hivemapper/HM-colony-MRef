# import json
# from itertools import chain
import numpy as np
import pandas as pd
import scipy.sparse as sparse


geo_matches_file   = "data/geometric_matches"
geo_matches_text   = "data/geometric_matches_decomp.txt"


# Reading the file
with open(geo_matches_file,'r') as matches_data:
  data = matches_data.readlines()
# trim preamble and brackets
data = data[3:-1]
adj = np.zeros(shape=(len(data),3))
for iadj, dat in enumerate(data):
  i,j = dat.replace("n","").strip().split("--")
  entry = [int(i), int(j), 1]
  adj[iadj,:] = entry

ndim = int(adj.max(axis=0)[:2].max()+1)
coo = sparse.coo_matrix((adj[:, 2], (adj[:, 0], adj[:, 1])), shape=[ndim,ndim],
                        dtype=int)

xx=coo.todense()

f = open(geo_matches_text, "w")
f.write("  adjacency<<   ")

for iw, wline in enumerate(xx):
  ll = str(wline).replace("\n","").split(']')[0].split('[')[-1].split()
  pl = ",".join(ll)
  f.write(pl)
  if iw==ndim-1:
    f.write(';\n    ')
  else:
    f.write(',\n')
f.close()


##  Example: geometric_matches_decomp.txt
#  adjacency<<   1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,
#    1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,
#    0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,
#    0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,
#    0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,
#    0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,
#    0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
#    0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,
#    0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,
#    0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,
#    0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,
#    0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,
#    0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,
#    0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,
#    0,0,0,0,0,0,0,0,0,0,0,0,0,1,1;
