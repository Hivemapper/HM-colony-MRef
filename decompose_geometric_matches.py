import json
# from itertools import chain
import numpy as np
import pandas as pd
import scipy.sparse as sparse

data_folder        = "data2/"
geo_matches_file   = data_folder+"geometric_matches"
geo_matches_text   = data_folder+"geometric_matches_decomp.txt"
sfm_data_file      = data_folder+"sfm_data.json"
# putative_matches_file   = "data/putative_matches"
# putative_matches_text   = "data/putative_matches_decomp.txt"


# Reading the file
with open(geo_matches_file,'r') as matches_data:
  data = matches_data.readlines()
# Reading the file
with open(sfm_data_file,'r') as sfm_data:
  sfm = json.load(sfm_data)

# orilist = open(data_folder+"orilist_2.txt", "w")
# imglist = open(data_folder+"imglist_2.txt", "w")

views = sfm['views']
sfm_map = {}
img_map = {}
for view in views:
  v_id     = int(view['key'])
  png_name = view['value']['ptr_wrapper']['data']['filename']
  png_id   = int(png_name.split('.')[0])
  sfm_map[v_id] = png_id -1 #pngs are indexed by 1, whereas "keys" and adj matrix are indexed at 0
  img_map[v_id] = png_name #pngs are indexed by 1, whereas "keys" and adj matrix are indexed at 0
  # orilist.write("ori/{0}.or\n".format(png_id))
  # imglist.write("img/{0}\n".format(png_name))
# orilist.close()
# imglist.close()


# trim preamble and brackets
data = data[2:-1]
adj = np.zeros(shape=(len(data),3))
for iadj, dat in enumerate(data):
  i,j = dat.replace("n","").strip().split("--")
  # entry = [int(i), int(j), 1]
  entry = [sfm_map[int(i)], sfm_map[int(j)], 1]
  adj[iadj,:] = entry

ndim = int(adj.max(axis=0)[:2].max()+1)
coo = sparse.coo_matrix((adj[:, 2], (adj[:, 0], adj[:, 1])), shape=[ndim,ndim],
                        dtype=int)

xx=coo.todense()
zz=xx+xx.T
# zz=np.triu(zz,0) 

f = open(geo_matches_text, "w")
f.write("  adjacency<<   ")

for iw, wline in enumerate(zz):
  ll = str(wline).replace("\n","").split(']')[0].split('[')[-1].split()
  pl = ",".join(ll)
  f.write(pl)
  if iw==ndim-1:
    f.write(';\n    ')
  else:
    f.write(',\n      ')
f.close()


##  Example: geometric_matches_decomp.txt
#  adjacency<<   
#    1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,
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
