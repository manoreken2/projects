#!/bin/bash -bx

#for i in $(seq 1 17); do
#	n1=$(printf "%04d" $i)
#	n2=$(printf "%04d" $((i+1)) )
#	python Run_FeatureMatch.py --img1 cylinder_img/$n1".png" --img2 cylinder_img/$n2".png" --result_csv $n1"_"$n2".csv"
#done

#for i in $(seq 1 16); do
#	n1=$(printf "%04d" $i)
#	n2=$(printf "%04d" $((i+2)) )
#	python Run_FeatureMatch.py --img1 cylinder_img/$n1".png" --img2 cylinder_img/$n2".png" --result_csv $n1"_"$n2".csv"
#done

#for i in 7; do
for i in $(seq 1 18); do
	for j in $(seq 1 18); do
	    if [ $i -ne $j ] ;then
	        n1=$(printf "%04d" $i)
	        n2=$(printf "%04d" $j )
	        python Run_TwoCam_Ransac.py --matched_point2d_csv $n1"_"$n2".csv" --result_ply "Result_"$n1"_"$n2".ply" --result_second_cam_csv "Result_"$n1"_"$n2"_SecondCam.csv" --result_two_cam_ply "Result_"$n1"_"$n2"_Est.ply" --result_two_cam_focallengths_csv="Result_"$n1"_"$n2"_FocalLengths.csv"
            #python Run_TwoCam_LeastSquare.py --matched_point2d_csv $n1"_"$n2".csv"
		fi
	done
done



