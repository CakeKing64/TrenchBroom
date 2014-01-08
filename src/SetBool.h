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

#ifndef __TrenchBroom__SetBool__
#define __TrenchBroom__SetBool__

#include <cassert>
#include <iostream>

namespace TrenchBroom {
    class SetBool {
    private:
        bool& m_value;
        bool m_setTo;
    public:
        SetBool(bool& value, bool setTo = true);
        ~SetBool();
    };

    template <typename R>
    class SetBoolFun {
    private:
        typedef void (R::*F)(bool b);
        R* m_receiver;
        F m_function;
        bool m_setTo;
    public:
        SetBoolFun(R* receiver, F function, bool setTo = true) :
        m_receiver(receiver),
        m_function(function),
        m_setTo(setTo) {
            assert(m_receiver != NULL);
            (m_receiver->*m_function)(m_setTo);
        }
        
        ~SetBoolFun() {
            (m_receiver->*m_function)(!m_setTo);
        }
    };
}

#endif /* defined(__TrenchBroom__SetBool__) */
