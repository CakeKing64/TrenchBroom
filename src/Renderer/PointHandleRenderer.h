/*
 Copyright (C) 2010-2014 Kristian Duske
 
 This file is part of TrenchBroom.
 
 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TrenchBroom__PointHandleRenderer__
#define __TrenchBroom__PointHandleRenderer__

#include "TrenchBroom.h"
#include "VecMath.h"
#include "Color.h"
#include "Renderer/Circle.h"
#include "Renderer/Sphere.h"

namespace TrenchBroom {
    namespace Renderer {
        class ActiveShader;
        class RenderContext;
        class Vbo;
        
        class PointHandleRenderer {
        private:
            Vbo& m_vbo;
            Sphere m_sphere;
            Circle m_circle;
            
            Vec3f::List m_positions;
            Color m_color;
            Color m_occludedColor;
            Color m_highlightColor;
            bool m_renderOccluded;
        public:
            PointHandleRenderer(Vbo& vbo, float radius = 3.0f, size_t iterations = 1);
            
            void setRadius(float radius, size_t iterations);
            void setPositions(Vec3f::List& positions);
            void setColor(const Color& color);
            void setOccludedColor(const Color& occludedColor);
            void setHighlightColor(const Color& highlightColor);
            void setRenderOccluded(bool renderOccluded);
            
            void renderSingleHandle(RenderContext& renderContext, const Vec3f& position);
            void renderMultipleHandles(RenderContext& renderContext, const Vec3f::List& positions);
            void renderStoredHandles(RenderContext& renderContext);
            
            void renderHandleHighlight(RenderContext& renderContext, const Vec3f& position);
        private:
            void prepare();
            void setupHandle(RenderContext& renderContext, ActiveShader& shader);
            void renderHandle(const Vec3f& position, ActiveShader& shader);
        };
    }
}

#endif /* defined(__TrenchBroom__PointHandleRenderer__) */
