/*
Copyright (c) 2003-2010 Sony Pictures Imageworks Inc., et al.
All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of Sony Pictures Imageworks nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <OpenColorSpace/OpenColorSpace.h>

#include "Lut3DOp.h"

#include <cmath>
#include <sstream>

OCS_NAMESPACE_ENTER
{
    namespace
    {
        inline float lerp(float a, float b, float z)
            { return (b - a) * z + a; }
        
        inline float lerp(float a, float b, float c, float d, float y, float z)
            { return lerp(lerp(a, b, z), lerp(c, d, z), y); }
        
        inline float lerp(float a, float b, float c, float d, float e, float f, float g, float h, float x, float y, float z)
            { return lerp(lerp(a,b,c,d,y,z), lerp(e,f,g,h,y,z), x); }
        
        inline float lookupNearest_3D(int rIndex, int gIndex, int bIndex,
                                      int size_red, int size_green, int size_blue,
                                      const float* simple_rgb_lut, int channelIndex)
        {
            return simple_rgb_lut[ Lut3DArrayOffset(rIndex, gIndex, bIndex, size_red, size_green, size_blue) + channelIndex];
        }
        
        // Note: This function assumes that minVal is less than maxVal
        inline int clamp(float k, float minVal, float maxVal)
        {
            return static_cast<int>(roundf(std::max(std::min(k, maxVal), minVal)));
        }
        
        ///////////////////////////////////////////////////////////////////////
        // Nearest Forward
        
        void Lut3D_Nearest(float* rgbaBuffer, long numPixels, const Lut3D & lut)
        {
            float maxIndex[3];
            float mInv[3];
            float b[3];
            float mInv_x_maxIndex[3];
            int lutSize[3];
            const float* startPos = &(lut.lut[0]);
            
            for(int i=0; i<3; ++i)
            {
                maxIndex[i] = (float) (lut.size[i] - 1);
                mInv[i] = 1.0f / (lut.from_max[i] - lut.from_min[i]);
                b[i] = lut.from_min[i];
                mInv_x_maxIndex[i] = (float) (mInv[i] * maxIndex[i]);
                
                lutSize[i] = lut.size[i];
            }
            
            int localIndex[3];
            
            for(long pixelIndex=0; pixelIndex<numPixels; ++pixelIndex)
            {
                localIndex[0] = clamp(mInv_x_maxIndex[0] * (rgbaBuffer[0] - b[0]), 0.0, maxIndex[0]);
                localIndex[1] = clamp(mInv_x_maxIndex[1] * (rgbaBuffer[1] - b[1]), 0.0, maxIndex[1]);
                localIndex[2] = clamp(mInv_x_maxIndex[2] * (rgbaBuffer[2] - b[2]), 0.0, maxIndex[2]);
                
                rgbaBuffer[0] = lookupNearest_3D(localIndex[0], localIndex[1], localIndex[2],
                                                 lutSize[0], lutSize[1], lutSize[2], startPos, 0);
                rgbaBuffer[1] = lookupNearest_3D(localIndex[0], localIndex[1], localIndex[2],
                                                 lutSize[0], lutSize[1], lutSize[2], startPos, 1);
                rgbaBuffer[2] = lookupNearest_3D(localIndex[0], localIndex[1], localIndex[2],
                                                 lutSize[0], lutSize[1], lutSize[2], startPos, 2);
                
                rgbaBuffer += 4;
            }
        }
        
        ///////////////////////////////////////////////////////////////////////
        // Linear Forward
        /*
        inline float lookupLinear_3D(float index, float maxIndex, const float * simple_lut)
        {
            int indexLow = clamp(std::floor(index), 0.0f, maxIndex);
            int indexHigh = clamp(std::ceil(index), 0.0f, maxIndex);
            float delta = index - (float)indexLow;
            return (1.0f-delta) * simple_lut[indexLow] + delta * simple_lut[indexHigh];
        }
        */
        
        void Lut3D_Linear(float* rgbaBuffer, long numPixels, const Lut3D & lut)
        {
            float maxIndex[3];
            float mInv[3];
            float b[3];
            float mInv_x_maxIndex[3];
            int lutSize[3];
            const float* startPos = &(lut.lut[0]);
            
            for(int i=0; i<3; ++i)
            {
                maxIndex[i] = (float) (lut.size[i] - 1);
                mInv[i] = 1.0f / (lut.from_max[i] - lut.from_min[i]);
                b[i] = lut.from_min[i];
                mInv_x_maxIndex[i] = (float) (mInv[i] * maxIndex[i]);
                
                lutSize[i] = lut.size[i];
            }
            
            float localIndex[3];
            int indexLow[3];
            int indexHigh[3];
            float delta[3];
            
            
            for(long pixelIndex=0; pixelIndex<numPixels; ++pixelIndex)
            {
                localIndex[0] = std::max(std::min(rgbaBuffer[0] * maxIndex[0], maxIndex[2]), 0.0f);
                localIndex[1] = std::max(std::min(rgbaBuffer[1] * maxIndex[1], maxIndex[2]), 0.0f);
                localIndex[2] = std::max(std::min(rgbaBuffer[2] * maxIndex[2], maxIndex[2]), 0.0f);
                
                indexLow[0] =  static_cast<int>(std::floor(localIndex[0]));
                indexLow[1] =  static_cast<int>(std::floor(localIndex[1]));
                indexLow[2] =  static_cast<int>(std::floor(localIndex[2]));
                
                // TODO: Confirm use of ceil, when local index is a maximum
                // does not clip above the max (and cause an out of bounds access)
                
                indexHigh[0] =  static_cast<int>(std::ceil(localIndex[0]));
                indexHigh[1] =  static_cast<int>(std::ceil(localIndex[1]));
                indexHigh[2] =  static_cast<int>(std::ceil(localIndex[2]));
                
                delta[0] = localIndex[0] - static_cast<float>(indexLow[0]);
                delta[1] = localIndex[1] - static_cast<float>(indexLow[1]);
                delta[2] = localIndex[2] - static_cast<float>(indexLow[2]);
                
                // TODO: Refactor to make lookups work on rgb triple rather
                //       than single components.
                
                rgbaBuffer[0] = lerp(lookupNearest_3D(indexLow[0],  indexLow[1],  indexLow[2],  lutSize[0], lutSize[1], lutSize[2], startPos, 0),
                                     lookupNearest_3D(indexLow[0],  indexLow[1],  indexHigh[2], lutSize[0], lutSize[1], lutSize[2], startPos, 0),
                                     lookupNearest_3D(indexLow[0],  indexHigh[1], indexLow[2],  lutSize[0], lutSize[1], lutSize[2], startPos, 0),
                                     lookupNearest_3D(indexLow[0],  indexHigh[1], indexHigh[2], lutSize[0], lutSize[1], lutSize[2], startPos, 0),
                                     lookupNearest_3D(indexHigh[0], indexLow[1],  indexLow[2],  lutSize[0], lutSize[1], lutSize[2], startPos, 0),
                                     lookupNearest_3D(indexHigh[0], indexLow[1],  indexHigh[2], lutSize[0], lutSize[1], lutSize[2], startPos, 0),
                                     lookupNearest_3D(indexHigh[0], indexHigh[1], indexLow[2],  lutSize[0], lutSize[1], lutSize[2], startPos, 0),
                                     lookupNearest_3D(indexHigh[0], indexHigh[1], indexHigh[2], lutSize[0], lutSize[1], lutSize[2], startPos, 0),
                                     delta[0], delta[1], delta[2]);
                
                rgbaBuffer[1] = lerp(lookupNearest_3D(indexLow[0],  indexLow[1],  indexLow[2],  lutSize[0], lutSize[1], lutSize[2], startPos, 1),
                                     lookupNearest_3D(indexLow[0],  indexLow[1],  indexHigh[2], lutSize[0], lutSize[1], lutSize[2], startPos, 1),
                                     lookupNearest_3D(indexLow[0],  indexHigh[1], indexLow[2],  lutSize[0], lutSize[1], lutSize[2], startPos, 1),
                                     lookupNearest_3D(indexLow[0],  indexHigh[1], indexHigh[2], lutSize[0], lutSize[1], lutSize[2], startPos, 1),
                                     lookupNearest_3D(indexHigh[0], indexLow[1],  indexLow[2],  lutSize[0], lutSize[1], lutSize[2], startPos, 1),
                                     lookupNearest_3D(indexHigh[0], indexLow[1],  indexHigh[2], lutSize[0], lutSize[1], lutSize[2], startPos, 1),
                                     lookupNearest_3D(indexHigh[0], indexHigh[1], indexLow[2],  lutSize[0], lutSize[1], lutSize[2], startPos, 1),
                                     lookupNearest_3D(indexHigh[0], indexHigh[1], indexHigh[2], lutSize[0], lutSize[1], lutSize[2], startPos, 1),
                                     delta[0], delta[1], delta[2]);
                
                rgbaBuffer[2] = lerp(lookupNearest_3D(indexLow[0],  indexLow[1],  indexLow[2],  lutSize[0], lutSize[1], lutSize[2], startPos, 2),
                                     lookupNearest_3D(indexLow[0],  indexLow[1],  indexHigh[2], lutSize[0], lutSize[1], lutSize[2], startPos, 2),
                                     lookupNearest_3D(indexLow[0],  indexHigh[1], indexLow[2],  lutSize[0], lutSize[1], lutSize[2], startPos, 2),
                                     lookupNearest_3D(indexLow[0],  indexHigh[1], indexHigh[2], lutSize[0], lutSize[1], lutSize[2], startPos, 2),
                                     lookupNearest_3D(indexHigh[0], indexLow[1],  indexLow[2],  lutSize[0], lutSize[1], lutSize[2], startPos, 2),
                                     lookupNearest_3D(indexHigh[0], indexLow[1],  indexHigh[2], lutSize[0], lutSize[1], lutSize[2], startPos, 2),
                                     lookupNearest_3D(indexHigh[0], indexHigh[1], indexLow[2],  lutSize[0], lutSize[1], lutSize[2], startPos, 2),
                                     lookupNearest_3D(indexHigh[0], indexHigh[1], indexHigh[2], lutSize[0], lutSize[1], lutSize[2], startPos, 2),
                                     delta[0], delta[1], delta[2]);
                
                rgbaBuffer += 4;
            }
        }
    }
    
    namespace
    {
        class Lut3DOp : public Op
        {
        public:
            Lut3DOp(Lut3DRcPtr lut,
                    Interpolation interpolation,
                    TransformDirection direction);
            virtual ~Lut3DOp();
            virtual void process(float* rgbaBuffer, long numPixels) const;
        
        private:
            Lut3DRcPtr m_lut;
            Interpolation m_interpolation;
            TransformDirection m_direction;
        };
        
        typedef SharedPtr<Lut3DOp> Lut3DOpRcPtr;
        
        Lut3DOp::Lut3DOp(Lut3DRcPtr lut,
                         Interpolation interpolation,
                         TransformDirection direction):
                            Op(),
                            m_lut(lut),
                            m_interpolation(interpolation),
                            m_direction(direction)
        {
            // TODO: assert luts is not 0 sized
            // Optionally, move this to a separate safety-check pass
            // to allow for optimizations to potentially remove
            // this pass.  For example, an inverse 3d lut may
            // not be allowed, but 2 in a row (forward + inverse)
            // may be allowed!
        }
        
        Lut3DOp::~Lut3DOp()
        { }
        
        void Lut3DOp::process(float* rgbaBuffer, long numPixels) const
        {
            if(m_direction == TRANSFORM_DIR_FORWARD)
            {
                if(m_interpolation == INTERP_NEAREST)
                {
                    Lut3D_Nearest(rgbaBuffer, numPixels, *m_lut);
                }
                else if(m_interpolation == INTERP_LINEAR)
                {
                    Lut3D_Linear(rgbaBuffer, numPixels, *m_lut);
                }
            }
            else if(m_direction == TRANSFORM_DIR_INVERSE)
            {
                throw OCSException("3D Luts cannot be applied in the inverse.");
            }
        }
    }
    
    void CreateLut3DOp(OpRcPtrVec * opVec,
                       Lut3DRcPtr lut,
                       Interpolation interpolation,
                       TransformDirection direction)
    {
        opVec->push_back( Lut3DOpRcPtr(new Lut3DOp(lut, interpolation, direction)) );
    }

}
OCS_NAMESPACE_EXIT