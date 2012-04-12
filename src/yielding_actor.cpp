/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#include "cppa/detail/yielding_actor.hpp"
#ifndef CPPA_DISABLE_CONTEXT_SWITCHING

#include <iostream>

#include "cppa/cppa.hpp"
#include "cppa/self.hpp"
#include "cppa/detail/invokable.hpp"

namespace cppa { namespace detail {

yielding_actor::~yielding_actor()
{
    delete m_behavior;
}

void yielding_actor::run(void* ptr_arg)
{
    auto this_ptr = reinterpret_cast<yielding_actor*>(ptr_arg);
    auto behavior_ptr = this_ptr->m_behavior;
    if (behavior_ptr)
    {
        bool cleanup_called = false;
        try { behavior_ptr->act(); }
        catch (actor_exited&)
        {
            // cleanup already called by scheduled_actor::quit
            cleanup_called = true;
        }
        catch (...)
        {
            this_ptr->cleanup(exit_reason::unhandled_exception);
            cleanup_called = true;
        }
        if (!cleanup_called) this_ptr->cleanup(exit_reason::normal);
        try { behavior_ptr->on_exit(); }
        catch (...) { }
    }
    yield(yield_state::done);
}


void yielding_actor::yield_until_not_empty()
{
    if (m_mailbox.can_fetch_more() == false)
    {
        m_state.store(abstract_scheduled_actor::about_to_block);
        CPPA_MEMORY_BARRIER();
        // make sure mailbox is 'empty'
        if (m_mailbox.can_fetch_more() == false)
        {
            m_state.store(abstract_scheduled_actor::ready);
            return;
        }
        else
        {
            yield(yield_state::blocked);
        }
    }
}

void yielding_actor::dequeue(partial_function& fun)
{
    auto iter = m_mailbox.cache().begin();
    auto mbox_end = m_mailbox.cache().end();
    for (;;)
    {
        for ( ; iter != mbox_end; ++iter)
        {
            if (dq(iter, fun) == dq_done)
            {
                m_mailbox.cache().erase(iter);
                return;
            }
        }
        yield_until_not_empty();
        iter = m_mailbox.try_fetch_more();
    }
}

void yielding_actor::dequeue(behavior& bhvr)
{
    if (bhvr.timeout().valid())
    {
        // try until a message was successfully dequeued
        request_timeout(bhvr.timeout());
        auto iter = m_mailbox.cache().begin();
        auto mbox_end = m_mailbox.cache().end();
        for (;;)
        {
            while (iter != mbox_end)
            {
                switch (dq(iter, bhvr.get_partial_function()))
                {
                    case dq_timeout_occured:
                        bhvr.handle_timeout();
                        // fall through
                    case dq_done:
                        iter = m_mailbox.cache().erase(iter);
                        return;
                    default:
                        ++iter;
                        break;
                }
            }
            yield_until_not_empty();
            iter = m_mailbox.try_fetch_more();
        }
    }
    else
    {
        // suppress virtual function call
        yielding_actor::dequeue(bhvr.get_partial_function());
    }
}

void yielding_actor::resume(util::fiber* from, resume_callback* callback)
{
    self.set(this);
    for (;;)
    {
        switch (call(&m_fiber, from))
        {
            case yield_state::done:
            {
                callback->exec_done();
                return;
            }
            case yield_state::ready:
            {
                break;
                //if (callback->still_ready()) break;
                //else return;
            }
            case yield_state::blocked:
            {
                switch (compare_exchange_state(abstract_scheduled_actor::about_to_block,
                                               abstract_scheduled_actor::blocked))
                {
                    case abstract_scheduled_actor::ready:
                    {
                        break;
                        //if (callback->still_ready()) break;
                        //else return;
                    }
                    case abstract_scheduled_actor::blocked:
                    {
                        // wait until someone re-schedules that actor
                        return;
                    }
                    default:
                    {
                        // illegal state
                        exit(7);
                    }
                }
                break;
            }
            default:
            {
                // illegal state
                exit(8);
            }
        }
    }
}

} } // namespace cppa::detail

#else // ifdef CPPA_DISABLE_CONTEXT_SWITCHING

namespace { int keep_compiler_happy() { return 42; } }

#endif // ifdef CPPA_DISABLE_CONTEXT_SWITCHING