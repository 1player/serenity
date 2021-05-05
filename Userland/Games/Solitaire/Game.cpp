/*
 * Copyright (c) 2020, Till Mayer <till.mayer@web.de>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Game.h"
#include <LibGUI/Painter.h>
#include <time.h>

REGISTER_WIDGET(Solitaire, Game);

namespace Solitaire {

static const Color s_background_color { Color::from_rgb(0x008000) };
static constexpr uint8_t new_game_animation_delay = 5;
static constexpr int s_timer_interval_ms = 1000 / 60;

Game::Game()
{
    m_stacks[Stock] = CardStack({ 10, 10 }, CardStack::Type::Stock);
    m_stacks[Waste] = CardStack({ 10 + Card::width + 10, 10 }, CardStack::Type::Waste);
    m_stacks[Foundation4] = CardStack({ Game::width - Card::width - 10, 10 }, CardStack::Type::Foundation);
    m_stacks[Foundation3] = CardStack({ Game::width - 2 * Card::width - 20, 10 }, CardStack::Type::Foundation);
    m_stacks[Foundation2] = CardStack({ Game::width - 3 * Card::width - 30, 10 }, CardStack::Type::Foundation);
    m_stacks[Foundation1] = CardStack({ Game::width - 4 * Card::width - 40, 10 }, CardStack::Type::Foundation);
    m_stacks[Pile1] = CardStack({ 10, 10 + Card::height + 10 }, CardStack::Type::Normal);
    m_stacks[Pile2] = CardStack({ 10 + Card::width + 10, 10 + Card::height + 10 }, CardStack::Type::Normal);
    m_stacks[Pile3] = CardStack({ 10 + 2 * Card::width + 20, 10 + Card::height + 10 }, CardStack::Type::Normal);
    m_stacks[Pile4] = CardStack({ 10 + 3 * Card::width + 30, 10 + Card::height + 10 }, CardStack::Type::Normal);
    m_stacks[Pile5] = CardStack({ 10 + 4 * Card::width + 40, 10 + Card::height + 10 }, CardStack::Type::Normal);
    m_stacks[Pile6] = CardStack({ 10 + 5 * Card::width + 50, 10 + Card::height + 10 }, CardStack::Type::Normal);
    m_stacks[Pile7] = CardStack({ 10 + 6 * Card::width + 60, 10 + Card::height + 10 }, CardStack::Type::Normal);
}

Game::~Game()
{
}

static float rand_float()
{
    return rand() / static_cast<float>(RAND_MAX);
}

void Game::timer_event(Core::TimerEvent&)
{
    if (m_game_over_animation) {
        VERIFY(!m_animation.card().is_null());
        if (m_animation.card()->position().x() > Game::width || m_animation.card()->rect().right() < 0)
            create_new_animation_card();

        m_animation.tick();
    }

    if (m_has_to_repaint || m_game_over_animation || m_new_game_animation) {
        m_repaint_all = false;
        update();
    }
}

void Game::create_new_animation_card()
{
    srand(time(nullptr));

    auto card = Card::construct(static_cast<Card::Type>(rand() % Card::Type::__Count), rand() % Card::card_count);
    card->set_position({ rand() % (Game::width - Card::width), rand() % (Game::height / 8) });

    int x_sgn = card->position().x() > (Game::width / 2) ? -1 : 1;
    m_animation = Animation(card, rand_float() + .4f, x_sgn * ((rand() % 3) + 2), .6f + rand_float() * .4f);
}

void Game::start_game_over_animation()
{
    if (m_game_over_animation)
        return;

    create_new_animation_card();
    m_game_over_animation = true;
}

void Game::stop_game_over_animation()
{
    if (!m_game_over_animation)
        return;

    m_game_over_animation = false;
    update();
}

void Game::setup()
{
    stop_game_over_animation();
    stop_timer();

    for (auto& stack : m_stacks)
        stack.clear();

    m_new_deck.clear();
    m_new_game_animation_pile = 0;
    m_score = 0;
    update_score(0);

    for (int i = 0; i < Card::card_count; ++i) {
        m_new_deck.append(Card::construct(Card::Type::Clubs, i));
        m_new_deck.append(Card::construct(Card::Type::Spades, i));
        m_new_deck.append(Card::construct(Card::Type::Hearts, i));
        m_new_deck.append(Card::construct(Card::Type::Diamonds, i));
    }

    srand(time(nullptr));
    for (uint8_t i = 0; i < 200; ++i)
        m_new_deck.append(m_new_deck.take(rand() % m_new_deck.size()));

    m_new_game_animation = true;
    start_timer(s_timer_interval_ms);
    update();
}

void Game::update_score(int to_add)
{
    m_score = max(static_cast<int>(m_score) + to_add, 0);

    if (on_score_update)
        on_score_update(m_score);
}

void Game::keydown_event(GUI::KeyEvent& event)
{
    if (m_new_game_animation || m_game_over_animation)
        return;

    if (event.key() == KeyCode::Key_F12)
        start_game_over_animation();
}

void Game::mousedown_event(GUI::MouseEvent& event)
{
    GUI::Frame::mousedown_event(event);

    if (m_new_game_animation || m_game_over_animation)
        return;

    auto click_location = event.position();
    for (auto& to_check : m_stacks) {
        if (to_check.bounding_box().contains(click_location)) {
            if (to_check.type() == CardStack::Type::Stock) {
                auto& waste = stack(Waste);
                auto& stock = stack(Stock);

                if (stock.is_empty()) {
                    if (waste.is_empty())
                        return;

                    while (!waste.is_empty()) {
                        auto card = waste.pop();
                        stock.push(card);
                    }

                    stock.set_dirty();
                    waste.set_dirty();
                    m_has_to_repaint = true;
                    update_score(-100);
                } else {
                    move_card(stock, waste);
                }
            } else if (!to_check.is_empty()) {
                auto& top_card = to_check.peek();

                if (top_card.is_upside_down()) {
                    if (top_card.rect().contains(click_location)) {
                        top_card.set_upside_down(false);
                        to_check.set_dirty();
                        update_score(5);
                        m_has_to_repaint = true;
                    }
                } else if (m_focused_cards.is_empty()) {
                    to_check.add_all_grabbed_cards(click_location, m_focused_cards);
                    m_mouse_down_location = click_location;
                    to_check.set_focused(true);
                    m_focused_stack = &to_check;
                    m_mouse_down = true;
                }
            }
            break;
        }
    }
}

void Game::mouseup_event(GUI::MouseEvent& event)
{
    GUI::Frame::mouseup_event(event);

    if (!m_focused_stack || m_focused_cards.is_empty() || m_game_over_animation || m_new_game_animation)
        return;

    bool rebound = true;
    for (auto& stack : m_stacks) {
        if (stack.is_focused())
            continue;

        for (auto& focused_card : m_focused_cards) {
            if (stack.bounding_box().intersects(focused_card.rect())) {
                if (stack.is_allowed_to_push(m_focused_cards.at(0))) {
                    for (auto& to_intersect : m_focused_cards) {
                        mark_intersecting_stacks_dirty(to_intersect);
                        stack.push(to_intersect);
                        m_focused_stack->pop();
                    }

                    m_focused_stack->set_dirty();
                    stack.set_dirty();

                    if (m_focused_stack->type() == CardStack::Type::Waste && stack.type() == CardStack::Type::Normal) {
                        update_score(5);
                    } else if (m_focused_stack->type() == CardStack::Type::Waste && stack.type() == CardStack::Type::Foundation) {
                        update_score(10);
                    } else if (m_focused_stack->type() == CardStack::Type::Normal && stack.type() == CardStack::Type::Foundation) {
                        update_score(10);
                    } else if (m_focused_stack->type() == CardStack::Type::Foundation && stack.type() == CardStack::Type::Normal) {
                        update_score(-15);
                    }

                    rebound = false;
                    break;
                }
            }
        }
    }

    if (rebound) {
        for (auto& to_intersect : m_focused_cards)
            mark_intersecting_stacks_dirty(to_intersect);

        m_focused_stack->rebound_cards();
        m_focused_stack->set_dirty();
    }

    m_mouse_down = false;
    m_has_to_repaint = true;
}

void Game::mousemove_event(GUI::MouseEvent& event)
{
    GUI::Frame::mousemove_event(event);

    if (!m_mouse_down || m_game_over_animation || m_new_game_animation)
        return;

    auto click_location = event.position();
    int dx = click_location.dx_relative_to(m_mouse_down_location);
    int dy = click_location.dy_relative_to(m_mouse_down_location);

    for (auto& to_intersect : m_focused_cards) {
        mark_intersecting_stacks_dirty(to_intersect);
        to_intersect.rect().translate_by(dx, dy);
    }

    m_mouse_down_location = click_location;
    m_has_to_repaint = true;
}

void Game::doubleclick_event(GUI::MouseEvent& event)
{
    GUI::Frame::doubleclick_event(event);

    if (m_game_over_animation) {
        start_game_over_animation();
        setup();
        return;
    }

    if (m_new_game_animation)
        return;

    auto click_location = event.position();
    for (auto& to_check : m_stacks) {
        if (to_check.type() == CardStack::Type::Foundation || to_check.type() == CardStack::Type::Stock)
            continue;

        if (to_check.bounding_box().contains(click_location) && !to_check.is_empty()) {
            auto& top_card = to_check.peek();
            if (!top_card.is_upside_down() && top_card.rect().contains(click_location)) {
                if (stack(Foundation1).is_allowed_to_push(top_card))
                    move_card(to_check, stack(Foundation1));
                else if (stack(Foundation2).is_allowed_to_push(top_card))
                    move_card(to_check, stack(Foundation2));
                else if (stack(Foundation3).is_allowed_to_push(top_card))
                    move_card(to_check, stack(Foundation3));
                else if (stack(Foundation4).is_allowed_to_push(top_card))
                    move_card(to_check, stack(Foundation4));
                else
                    break;

                update_score(10);
            }
            break;
        }
    }

    m_has_to_repaint = true;
}

void Game::check_for_game_over()
{
    for (auto& stack : m_stacks) {
        if (stack.type() != CardStack::Type::Foundation)
            continue;
        if (stack.count() != Card::card_count)
            return;
    }

    start_game_over_animation();
}

void Game::move_card(CardStack& from, CardStack& to)
{
    auto card = from.pop();

    card->set_moving(true);
    m_focused_cards.clear();
    m_focused_cards.append(card);
    mark_intersecting_stacks_dirty(card);
    to.push(card);

    from.set_dirty();
    to.set_dirty();

    m_has_to_repaint = true;
}

void Game::mark_intersecting_stacks_dirty(Card& intersecting_card)
{
    for (auto& stack : m_stacks) {
        if (intersecting_card.rect().intersects(stack.bounding_box())) {
            stack.set_dirty();
            m_has_to_repaint = true;
        }
    }
}

void Game::paint_event(GUI::PaintEvent& event)
{
    GUI::Frame::paint_event(event);

    m_has_to_repaint = false;
    if (m_game_over_animation && m_repaint_all)
        return;

    GUI::Painter painter(*this);
    painter.add_clip_rect(frame_inner_rect());
    painter.add_clip_rect(event.rect());

    if (m_repaint_all) {
        painter.fill_rect(event.rect(), s_background_color);

        for (auto& stack : m_stacks)
            stack.draw(painter, s_background_color);
    } else if (m_game_over_animation && !m_animation.card().is_null()) {
        m_animation.card()->draw(painter);
    } else if (m_new_game_animation) {
        if (m_new_game_animation_delay < new_game_animation_delay) {
            ++m_new_game_animation_delay;
        } else {
            m_new_game_animation_delay = 0;
            auto& current_pile = stack(piles.at(m_new_game_animation_pile));

            if (current_pile.count() < m_new_game_animation_pile) {
                auto card = m_new_deck.take_last();
                card->set_upside_down(true);
                current_pile.push(card);
            } else {
                current_pile.push(m_new_deck.take_last());
                ++m_new_game_animation_pile;
            }
            current_pile.set_dirty();

            if (m_new_game_animation_pile == piles.size()) {
                while (!m_new_deck.is_empty())
                    stack(Stock).push(m_new_deck.take_last());
                stack(Stock).set_dirty();
                m_new_game_animation = false;
            }
        }
    }

    if (!m_game_over_animation && !m_repaint_all) {
        if (!m_focused_cards.is_empty()) {
            for (auto& focused_card : m_focused_cards)
                focused_card.clear(painter, s_background_color);
        }

        for (auto& stack : m_stacks) {
            if (stack.is_dirty())
                stack.draw(painter, s_background_color);
        }

        if (!m_focused_cards.is_empty()) {
            for (auto& focused_card : m_focused_cards) {
                focused_card.draw(painter);
                focused_card.save_old_position();
            }
        }
    }

    m_repaint_all = true;
    if (!m_mouse_down) {
        if (!m_focused_cards.is_empty()) {
            check_for_game_over();
            for (auto& card : m_focused_cards)
                card.set_moving(false);
            m_focused_cards.clear();
        }

        if (m_focused_stack) {
            m_focused_stack->set_focused(false);
            m_focused_stack = nullptr;
        }
    }
}

}