#!/bin/bash

# for (( i=1; i<=30; i++ ))
# do
#   echo "Iteration $i"
#   bazel run //hive/hive_scripts/lib/mesh:tile_mesh -- 38.47986 -122.71860 14 14 0 0 ~/pmesh_new_params 1 1 0 2>&1 | tee ~/pmesh_new_params/tile_mesh_$i.log
# done

model_id='5efe41c0505bb7b34c3fc3a1'
hostnme='mac-dev-127'
datafolder='/home/centos/MRef/data_sr_low'

mkdir $datafolder/temp
mkdir $datafolder/mesh
mkdir $datafolder/img
mkdir $datafolder/likeli
mkdir $datafolder/ori

bazel run //hive/hive_scripts/lib/mesh:mesh_extractor -- /home/centos/meshcam $model_id
bazel run //hive/hive_scripts/lib/mesh:single_model_mesh -- $model_id $datafolder/temp 0 1 0 2>&1 | tee $datafolder/temp/smm_010.log


sudo cp /mnt/HM/$hostname/hive/waggle/$model_id/reconstruction/matches.tar.gz $datafolder/temp
sudo cp /mnt/HM/$hostname/hive/waggle/$model_id/data/frames.tar.gz $datafolder/temp


tar -xf $datafolder/temp/matches.tar.gz -C $datafolder/temp
cp $datafolder/temp/geometric_matches $datafolder
cp $datafolder/temp/temp/sfm_data.json $datafolder

tar -xf $datafolder/temp/frames.tar.gz -C $datafolder/img


# for (( i=1; i<=30; i++ ))
# do
#   echo "Iteration $i"
#   bazel run //hive/hive_scripts/lib/mesh:tile_mesh -- 38.47986 -122.71860 14 14 0 0 ~/pmesh_new_params 1 1 0 2>&1 | tee ~/pmesh_new_params/tile_mesh_$i.log
# done

imgdir=$datafolder/img
likelidir=$datafolder/likeli

> $datafolder/segmentation_image_src.txt
> $datafolder/segmentation_image_dst.txt

# cd $imgdir
# for i in *.png 
# do
#   echo "$imgdir/$i" >> $datafolder/segmentation_image_src.txt
#   echo "$likelidir/$i" >> $datafolder/segmentation_image_dst.txt
# done

find $datafolder/img -name "*.png" > $datafolder/segmentation_image_src.txt
cp $datafolder/segmentation_image_src.txt $datafolder/segmentation_image_dst.txt


sed -i 's/.png/.jpg/g' $datafolder/segmentation_image_src.txt
sed -i 's/.png/.tif/g' $datafolder/segmentation_image_dst.txt
sed -i 's+/img/+/likeli/+g' $datafolder/segmentation_image_dst.txt

cp $datafolder/img/* $datafolder/likeli
cd $datafolder/likeli
gm mogrify -format jpg *.png
rm *.png
inference --image=$datafolder/segmentation_image_src.txt --results=$datafolder/segmentation_image_dst.txt --scale=100 --graph=segmentation_model.pb --root_dir=/usr/etc/hive/segmentation/;

cd $datafolder
find mesh -name "*.ply" > $datafolder/meshlist.txt


