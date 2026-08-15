#!/bin/bash -b

mkdir -p tmp

Feature_match() {
	for i in $(seq 1 17); do
		n1=$(printf "%04d" $i)
		n2=$(printf "%04d" $((i+1)) )
		python Run_FeatureMatch.py --img1 cylinder_img/$n1".png" --img2 cylinder_img/$n2".png" --result_csv tmp/$n1"_"$n2".csv" &
	done
	wait
}

Feature_match3() {
	camNum=24
	camNumMinus1=$((camNum - 1))

	for i in $(seq 0 $camNumMinus1); do
		i0=$i

		i1=$((i+1))
		if [ "$i1" -ge "$camNum" ]; then
			i1=$((i1 - $camNum))
		fi

		i2=$((i+2))
		if [ "$i2" -ge "$camNum" ]; then
			i2=$((i2 - $camNum))
		fi

		n1=$(printf "%04d" $i0 )
		n2=$(printf "%04d" $i1 )
		n3=$(printf "%04d" $i2 )
		echo python Run_FeatureMatch3.py --cam1id=$i0 --cam2id=$i1 --cam3id=$i2 --img1 Synthetic_OctPrism/$n1".png" --img2 Synthetic_OctPrism/$n2".png" --img3 Synthetic_OctPrism/$n3".png" --result_csv tmp/$n1"_"$n2"_"$n3".csv" &
	done
	wait
}

Pose_estimation() {
	for i in 7; do
		for i in $(seq 1 18); do
			for j in $(seq 1 18); do
				if [ $i -ne $j ] ;then
					n1=$(printf "%04d" $i)
					n2=$(printf "%04d" $j )
					python Run_TwoCam_Ransac.py --matched_point2d_csv tmp/$n1"_"$n2".csv" --result_ply "tmp/Result_"$n1"_"$n2".ply" --result_second_cam_csv "tmp/Result_"$n1"_"$n2"_SecondCam.csv" --result_two_cam_ply "Result_"$n1"_"$n2"_Est.ply" --result_two_cam_focallengths_csv="Result_"$n1"_"$n2"_FocalLengths.csv" &
				fi
			done
			wait
		done
	done
	wait
}

#Feature_match
#Pose_estimation

Feature_match3
