/*
  ==============================================================================

   This file is part of the JUCE library - "Jules' Utility Class Extensions"
   Copyright 2004-9 by Raw Material Software Ltd.

  ------------------------------------------------------------------------------

   JUCE can be redistributed and/or modified under the terms of the GNU General
   Public License (Version 2), as published by the Free Software Foundation.
   A copy of the license is included in the JUCE distribution, or can be found
   online at www.gnu.org/licenses.

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.rawmaterialsoftware.com/juce for more information.

  ==============================================================================
*/

#include "../../core/juce_StandardHeader.h"

BEGIN_JUCE_NAMESPACE


#include "juce_Desktop.h"
#include "juce_ComponentDeletionWatcher.h"
#include "../graphics/geometry/juce_RectangleList.h"

//==============================================================================
Desktop::Desktop() throw()
    : mouseClickCounter (0),
      mouseMovedSignificantlySincePressed (false),
      kioskModeComponent (0)
{
    zerostruct (mouseDowns);
    refreshMonitorSizes();
}

Desktop::~Desktop() throw()
{
    jassert (instance == this);
    instance = 0;

    // doh! If you don't delete all your windows before exiting, you're going to
    // be leaking memory!
    jassert (desktopComponents.size() == 0);
}

Desktop& JUCE_CALLTYPE Desktop::getInstance() throw()
{
    if (instance == 0)
        instance = new Desktop();

    return *instance;
}

Desktop* Desktop::instance = 0;

//==============================================================================
extern void juce_updateMultiMonitorInfo (Array <Rectangle<int> >& monitorCoords,
                                         const bool clipToWorkArea);

void Desktop::refreshMonitorSizes() throw()
{
    const Array <Rectangle<int> > oldClipped (monitorCoordsClipped);
    const Array <Rectangle<int> > oldUnclipped (monitorCoordsUnclipped);

    monitorCoordsClipped.clear();
    monitorCoordsUnclipped.clear();
    juce_updateMultiMonitorInfo (monitorCoordsClipped, true);
    juce_updateMultiMonitorInfo (monitorCoordsUnclipped, false);
    jassert (monitorCoordsClipped.size() > 0
              && monitorCoordsClipped.size() == monitorCoordsUnclipped.size());

    if (oldClipped != monitorCoordsClipped
         || oldUnclipped != monitorCoordsUnclipped)
    {
        for (int i = ComponentPeer::getNumPeers(); --i >= 0;)
        {
            ComponentPeer* const p = ComponentPeer::getPeer (i);
            if (p != 0)
                p->handleScreenSizeChange();
        }
    }
}

int Desktop::getNumDisplayMonitors() const throw()
{
    return monitorCoordsClipped.size();
}

const Rectangle<int> Desktop::getDisplayMonitorCoordinates (const int index, const bool clippedToWorkArea) const throw()
{
    return clippedToWorkArea ? monitorCoordsClipped [index]
                             : monitorCoordsUnclipped [index];
}

const RectangleList Desktop::getAllMonitorDisplayAreas (const bool clippedToWorkArea) const throw()
{
    RectangleList rl;

    for (int i = 0; i < getNumDisplayMonitors(); ++i)
        rl.addWithoutMerging (getDisplayMonitorCoordinates (i, clippedToWorkArea));

    return rl;
}

const Rectangle<int> Desktop::getMainMonitorArea (const bool clippedToWorkArea) const throw()
{
    return getDisplayMonitorCoordinates (0, clippedToWorkArea);
}

const Rectangle<int> Desktop::getMonitorAreaContaining (const Point<int>& position, const bool clippedToWorkArea) const throw()
{
    Rectangle<int> best (getMainMonitorArea (clippedToWorkArea));
    double bestDistance = 1.0e10;

    for (int i = getNumDisplayMonitors(); --i >= 0;)
    {
        const Rectangle<int> rect (getDisplayMonitorCoordinates (i, clippedToWorkArea));

        if (rect.contains (position))
            return rect;

        const double distance = rect.getCentre().getDistanceFrom (position);

        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = rect;
        }
    }

    return best;
}

//==============================================================================
int Desktop::getNumComponents() const throw()
{
    return desktopComponents.size();
}

Component* Desktop::getComponent (const int index) const throw()
{
    return desktopComponents [index];
}

Component* Desktop::findComponentAt (const Point<int>& screenPosition) const
{
    for (int i = desktopComponents.size(); --i >= 0;)
    {
        Component* const c = desktopComponents.getUnchecked(i);
        const Point<int> relative (c->globalPositionToRelative (screenPosition));

        if (c->contains (relative.getX(), relative.getY()))
            return c->getComponentAt (relative.getX(), relative.getY());
    }

    return 0;
}

//==============================================================================
void Desktop::addDesktopComponent (Component* const c) throw()
{
    jassert (c != 0);
    jassert (! desktopComponents.contains (c));
    desktopComponents.addIfNotAlreadyThere (c);
}

void Desktop::removeDesktopComponent (Component* const c) throw()
{
    desktopComponents.removeValue (c);
}

void Desktop::componentBroughtToFront (Component* const c) throw()
{
    const int index = desktopComponents.indexOf (c);
    jassert (index >= 0);

    if (index >= 0)
    {
        int newIndex = -1;

        if (! c->isAlwaysOnTop())
        {
            newIndex = desktopComponents.size();

            while (newIndex > 0 && desktopComponents.getUnchecked (newIndex - 1)->isAlwaysOnTop())
                --newIndex;

            --newIndex;
        }

        desktopComponents.move (index, newIndex);
    }
}

//==============================================================================
const Point<int> Desktop::getLastMouseDownPosition() throw()
{
    return getInstance().mouseDowns[0].position;
}

int Desktop::getMouseButtonClickCounter() throw()
{
    return getInstance().mouseClickCounter;
}

void Desktop::incrementMouseClickCounter() throw()
{
    ++mouseClickCounter;
}

const Time Desktop::getLastMouseDownTime() const throw()
{
    return Time (mouseDowns[0].time);
}

void Desktop::registerMouseDown (const Point<int>& position, int64 time, Component* component) throw()
{
    for (int i = numElementsInArray (mouseDowns); --i > 0;)
        mouseDowns[i] = mouseDowns[i - 1];

    mouseDowns[0].position = position;
    mouseDowns[0].time = time;
    mouseDowns[0].component = component;
    mouseMovedSignificantlySincePressed = false;
}

void Desktop::registerMouseDrag (const Point<int>& position) throw()
{
    mouseMovedSignificantlySincePressed
        = mouseMovedSignificantlySincePressed
           || abs (mouseDowns[0].position.getX() - position.getX()) >= 4
           || abs (mouseDowns[0].position.getY() - position.getY()) >= 4;
}

int Desktop::getNumberOfMultipleClicks() const throw()
{
    int numClicks = 0;

    if (mouseDowns[0].time != 0)
    {
        if (! mouseMovedSignificantlySincePressed)
            ++numClicks;

        for (int i = 1; i < numElementsInArray (mouseDowns); ++i)
        {
            if (mouseDowns[0].time - mouseDowns[i].time
                    < (int) (MouseEvent::getDoubleClickTimeout() * (1.0 + 0.25 * (i - 1)))
                && abs (mouseDowns[0].position.getX() - mouseDowns[i].position.getX()) < 8
                && abs (mouseDowns[0].position.getY() - mouseDowns[i].position.getY()) < 8
                && mouseDowns[0].component == mouseDowns[i].component)
            {
                ++numClicks;
            }
            else
            {
                break;
            }
        }
    }

    return numClicks;
}

//==============================================================================
void Desktop::addGlobalMouseListener (MouseListener* const listener) throw()
{
    jassert (listener != 0);

    if (listener != 0)
    {
        mouseListeners.add (listener);
        resetTimer();
    }
}

void Desktop::removeGlobalMouseListener (MouseListener* const listener) throw()
{
    mouseListeners.removeValue (listener);
    resetTimer();
}

//==============================================================================
void Desktop::addFocusChangeListener (FocusChangeListener* const listener) throw()
{
    jassert (listener != 0);

    if (listener != 0)
        focusListeners.add (listener);
}

void Desktop::removeFocusChangeListener (FocusChangeListener* const listener) throw()
{
    focusListeners.removeValue (listener);
}

void Desktop::triggerFocusCallback() throw()
{
    triggerAsyncUpdate();
}

void Desktop::handleAsyncUpdate()
{
    for (int i = focusListeners.size(); --i >= 0;)
    {
        ((FocusChangeListener*) focusListeners.getUnchecked (i))->globalFocusChanged (Component::getCurrentlyFocusedComponent());
        i = jmin (i, focusListeners.size());
    }
}

//==============================================================================
void Desktop::timerCallback()
{
    if (lastFakeMouseMove != getMousePosition())
        sendMouseMove();
}

void Desktop::sendMouseMove()
{
    if (mouseListeners.size() > 0)
    {
        startTimer (20);

        lastFakeMouseMove = getMousePosition();

        Component* const target = findComponentAt (lastFakeMouseMove);

        if (target != 0)
        {
            ComponentDeletionWatcher deletionChecker (target);
            const Point<int> pos (target->globalPositionToRelative (lastFakeMouseMove));
            const Time now (Time::getCurrentTime());

            const MouseEvent me (pos, ModifierKeys::getCurrentModifiers(),
                                 target, now, pos, now, 0, false);

            for (int i = mouseListeners.size(); --i >= 0;)
            {
                if (ModifierKeys::getCurrentModifiers().isAnyMouseButtonDown())
                    ((MouseListener*) mouseListeners[i])->mouseDrag (me);
                else
                    ((MouseListener*) mouseListeners[i])->mouseMove (me);

                if (deletionChecker.hasBeenDeleted())
                    return;

                i = jmin (i, mouseListeners.size());
            }
        }
    }
}

void Desktop::resetTimer() throw()
{
    if (mouseListeners.size() == 0)
        stopTimer();
    else
        startTimer (100);

    lastFakeMouseMove = getMousePosition();
}

//==============================================================================
extern void juce_setKioskComponent (Component* kioskModeComponent, bool enableOrDisable, bool allowMenusAndBars);

void Desktop::setKioskModeComponent (Component* componentToUse, const bool allowMenusAndBars)
{
    if (kioskModeComponent != componentToUse)
    {
        // agh! Don't delete a component without first stopping it being the kiosk comp
        jassert (kioskModeComponent == 0 || kioskModeComponent->isValidComponent());
        // agh! Don't remove a component from the desktop if it's the kiosk comp!
        jassert (kioskModeComponent == 0 || kioskModeComponent->isOnDesktop());

        if (kioskModeComponent->isValidComponent())
        {
            juce_setKioskComponent (kioskModeComponent, false, allowMenusAndBars);

            kioskModeComponent->setBounds (kioskComponentOriginalBounds);
        }

        kioskModeComponent = componentToUse;

        if (kioskModeComponent != 0)
        {
            jassert (kioskModeComponent->isValidComponent());

            // Only components that are already on the desktop can be put into kiosk mode!
            jassert (kioskModeComponent->isOnDesktop());

            kioskComponentOriginalBounds = kioskModeComponent->getBounds();

            juce_setKioskComponent (kioskModeComponent, true, allowMenusAndBars);
        }
    }
}


END_JUCE_NAMESPACE
