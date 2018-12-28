#ifndef HEADER_00818641_6D7B_4A3D_AFC6_38CC0DBAD99B_INCLUDED
#define HEADER_00818641_6D7B_4A3D_AFC6_38CC0DBAD99B_INCLUDED

namespace genny {

using ActorId = unsigned int;

class ActorContext;

/**
 * An Actor is the base unit of work in genny. An actor is a single-
 * threaded entity.
 *
 * The basic flow is:
 *
 * 1.  Load %YAML File
 * 2.  Construct metrics and other delegate objects
 * 3.  Call all enabled ActorProducers to produce as many Actors as they
 *     wish. Each ActorProducer is given each of the ActorContext objects.
 * 4.  Create a thread for each actor produced.
 * 5.  Call .run() for each actor.
 * 6.  Wait until all actors return from run().
 *
 * When writing a new Actor, there are two steps:
 *
 * 1.  Write the Actor subclass itself. Most actors should consider
 *     having a PhaseLoop member variable that they use for flow-control
 *     logic to collaborate cleanly with other actors.
 *
 * 2.  Write an `ActorProducer` that can produce an ActorVector from an
 *     ActorContext. The ActorProducer is where configuration values can be
 *     read and validated and passed into the Actor implementations.
 *     Typically `ActorProducer`s will simply be a static method on an Actor
 *     subclass.
 *
 * See other Actor implementations as an example. In addition there is the
 * `create-new-actor` script that assists with the boilerplate necessary to
 * create a new Actor instance.
 *
 * Actors may retain a reference to the ActorContext and/or parent
 * WorkloadContext, but it is recommended for performance that they
 * call `context.get(...)` only during their constructors and retain
 * refs or copies of config objects
 */
class Actor {


public:
    Actor(ActorContext& context);
    virtual ~Actor() = default;

    /**
     * A consistent compilation-unit unique name for this actor.
     * This name is mostly intended for metrics and logging purposes.
     */
    // static std::string_view defaultName()

    /**
     * The main method of an actor. Will be run in its own thread.
     * This is only intended to be called by workload drivers.
     */
    virtual void run() = 0;

    virtual ActorId id() const {
        return _id;
    }

private:
    ActorId _id;
};

}  // namespace genny

#endif  // HEADER_00818641_6D7B_4A3D_AFC6_38CC0DBAD99B_INCLUDED
