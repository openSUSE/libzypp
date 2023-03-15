/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPPNG_PROGRESSOBSERVER_H
#define ZYPPNG_PROGRESSOBSERVER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define ZYPP_TYPE_PROGRESS_OBSERVER zypp_progress_observer_get_type()
G_DECLARE_FINAL_TYPE (ZyppProgressObserver, zypp_progress_observer, ZYPP, PROGRESS_OBSERVER, GObject)

/**
 * zypp_progress_observer_add_subtask:
 * @self: a `ZyppProgressObserver`
 * @newChild: (transfer none): A reference to the new subtask, the parent takes a reference to it
 * @weight: The weight how the subtasks steps should be calculated into the parents percentage
 *
 * Registers a subtask as a child to the current one
 */
void zypp_progress_observer_add_subtask(ZyppProgressObserver *self, ZyppProgressObserver *newChild, gfloat weight );

/**
 * zypp_progress_observer_get_children:
 * @self: a `ZyppProgressObserver`
 *
 * Returns: (element-type ZyppProgressObserver) (transfer none): The direct children of this Task
 */
const GList *zypp_progress_observer_get_children ( ZyppProgressObserver *self );

/**
 * zypp_progress_observer_get_label:
 * @self: a `ZyppProgressObserver`
 *
 * Returns: (transfer none) (nullable): The task label or NULL if none was set
 */
const gchar *zypp_progress_observer_get_label ( ZyppProgressObserver *self );

/**
 * zypp_progress_observer_get_steps:
 * @self: a `ZyppProgressObserver`
 *
 * Returns: The effective number of steps for this `ZyppProgressObserver` including possible substasks
 */
gdouble zypp_progress_observer_get_steps    ( ZyppProgressObserver *self );

/**
 * zypp_progress_observer_get_base_steps:
 * @self: a `ZyppProgressObserver`
 *
 * Returns: The number of steps for this `ZyppProgressObserver` excluding possible substasks
 */
gint zypp_progress_observer_get_base_steps  ( ZyppProgressObserver *self );

/**
 * zypp_progress_observer_get_current:
 * @self: a `ZyppProgressObserver`
 *
 * Returns: The current effective value of the task progress including subtasks, by default this is 0
 */
gdouble zypp_progress_observer_get_current  ( ZyppProgressObserver *self );

/**
 * zypp_progress_observer_get_progress:
 * @self: a `ZyppProgressObserver`
 *
 * Returns: The current percentage value of the task progress including subtasks, by default this is 0
 */
gdouble zypp_progress_observer_get_progress ( ZyppProgressObserver *self );

/**
 * zypp_progress_observer_set_label:
 * @self: a `ZyppProgressObserver`
 * @label: (transfer none): The new label
 *
 * Changes the currently used label
 *
 */
void zypp_progress_observer_set_label    ( ZyppProgressObserver *self, const gchar *label );

/**
 * zypp_progress_observer_set_base_steps:
 * @self: a `ZyppProgressObserver`
 * @stepCount: New number of steps
 *
 * Changes the currently used nr of expected steps for this `ZyppProgressObserver`, if the given value is less than
 * 0 nothing is changed.
 *
 */
void zypp_progress_observer_set_base_steps    (ZyppProgressObserver *self, gint stepCount );

/**
 * zypp_progress_observer_set_value:
 * @self: a `ZyppProgressObserver`
 * @value: New current value
 *
 * Changes the current completed stepcount for this `ZyppProgressObserver` the subtasks will be added on top.
 */
void zypp_progress_observer_set_current  ( ZyppProgressObserver *self, gdouble value );

/**
 * zypp_progress_observer_set_finished:
 * @self: a `ZyppProgressObserver`
 *
 * Fills the progress and all its children up to 100% and emits the finished signal
 */
void zypp_progress_observer_set_finished ( ZyppProgressObserver *self );

/**
 * zypp_progress_observer_inc:
 * @self: a `ZyppProgressObserver`
 * @increase: Number of steps to increase the value
 *
 * Increases the current progress value by the number of given steps
 */
void zypp_progress_observer_inc   ( ZyppProgressObserver *self, gint increase );

// void zypp_progress_observer_begin ( ZyppProgressObserver *self );
// void zypp_progress_observer_end   ( ZyppProgressObserver *self );

G_END_DECLS

#ifdef  __cplusplus
#include <zyppng/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppProgressObserver, zypp_progress_observer, ZYPP, PROGRESS_OBSERVER )
#endif

#endif
