/****************************************************************************
 *
 * Copyright 2023 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

/**
 * @file TFLM.h
 * @brief AIEngine implementation for Tensor Flow Micro
 */

#ifndef __TFLM_H__
#define __TFLM_H__

#include <memory>
#include "aifw/aifw.h"
#include "AIEngine.h"

/* Tensorflow structure declaration */
struct TfLiteTensor;
namespace tflite {
struct Model;
class MicroInterpreter;
class ErrorReporter;
} /* namespace tflite */

namespace aifw {

class TFLM : public AIEngine
{
public:
	TFLM();
	~TFLM();
	AIFW_RESULT loadModel(const char *file);
	AIFW_RESULT loadModel(const unsigned char *model);
	void *invoke(void *inputData);

private:
	AIFW_RESULT _loadModel(void);

	size_t mTensorArenaSize;
	std::shared_ptr<uint8_t> mTensorArena;
	const tflite::Model *mModel;
	char *mBuf;
	std::shared_ptr<tflite::MicroInterpreter> mInterpreter;
	std::shared_ptr<tflite::ErrorReporter> mErrorReporter;
	TfLiteTensor *mInput;
	TfLiteTensor *mOutput;
	uint16_t mModelInputSize;
	uint16_t mModelOutputSize;
};

} /* namespace aifw */

#endif /* __TFLM_H__ */
