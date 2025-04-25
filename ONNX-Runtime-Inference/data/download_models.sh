#!/bin/bash

mkdir -p models
cd models

LINKS=(
    "https://github.com/onnx/models/raw/main/validated/vision/classification/alexnet/model/bvlcalexnet-12.onnx"
    "https://github.com/onnx/models/raw/main/validated/vision/classification/caffenet/model/caffenet-12.onnx"
    "https://github.com/onnx/models/raw/main/validated/vision/classification/densenet-121/model/densenet-12.onnx"
    "https://github.com/onnx/models/raw/main/validated/vision/classification/efficientnet-lite4/model/efficientnet-lite4-11.onnx"
    "https://github.com/onnx/models/raw/main/validated/vision/classification/inception_and_googlenet/googlenet/model/googlenet-12.onnx"
    "https://github.com/onnx/models/raw/main/validated/vision/classification/inception_and_googlenet/inception_v1/model/inception-v1-12.onnx"
    "https://github.com/onnx/models/raw/main/validated/vision/classification/inception_and_googlenet/inception_v2/model/inception-v2-9.onnx"
    "https://github.com/onnx/models/raw/main/validated/vision/classification/mobilenet/model/mobilenetv2-12.onnx"
    "https://github.com/onnx/models/raw/main/validated/vision/classification/rcnn_ilsvrc13/model/rcnn-ilsvrc13-9.onnx"
    "https://github.com/onnx/models/raw/main/validated/vision/classification/resnet/model/resnet50-v1-12.onnx"
    "https://github.com/onnx/models/raw/main/validated/vision/classification/shufflenet/model/shufflenet-v2-12.onnx"
    "https://github.com/onnx/models/raw/main/validated/vision/classification/squeezenet/model/squeezenet1.1-7.onnx"
    "https://github.com/onnx/models/raw/main/validated/vision/classification/vgg/model/vgg16-bn-7.onnx"
)

NUM_JOBS=6
job_count=0

download_model() {
    local URL="$1"
    local BASENAME=$(basename "$URL")

    if [ ! -f "$BASENAME" ]; then
        echo "Downloading model $BASENAME"
        wget "$URL" -O "$BASENAME"
    else
        echo "Model $BASENAME already exists, skipping download."
    fi
}

for URL in "${LINKS[@]}"; do
    download_model "$URL" &
    ((job_count++))

    if (( job_count >= NUM_JOBS )); then
        wait
        job_count=0
    fi
done

wait

echo "✅ All models downloaded."



