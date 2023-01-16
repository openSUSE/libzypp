#ifndef ZYPP_CORE_BASE_PRIVATE_CONFIGOPTION_P_H
#define ZYPP_CORE_BASE_PRIVATE_CONFIGOPTION_P_H

namespace zypp
{

 /** Mutable option. */
  template<class Tp>
      struct Option
      {
        typedef Tp value_type;

        /** No default ctor, explicit initialisation! */
        Option( value_type initial_r )
          : _val( std::move(initial_r) )
        {}

        Option & operator=( value_type newval_r )
        { set( std::move(newval_r) ); return *this; }

        /** Get the value.  */
        const value_type & get() const
        { return _val; }

        /** Autoconversion to value_type.  */
        operator const value_type &() const
        { return _val; }

        /** Set a new value.  */
        void set( value_type newval_r )
        { _val = std::move(newval_r); }

        private:
          value_type _val;
      };

  /** Mutable option with initial value also remembering a config value. */
  template<class Tp>
      struct DefaultOption : public Option<Tp>
      {
        typedef Tp         value_type;
        typedef Option<Tp> option_type;

        explicit DefaultOption( value_type initial_r )
        : Option<Tp>( initial_r )
        , _default( std::move(initial_r) )
        {}

        DefaultOption & operator=( value_type newval_r )
        { this->set( std::move(newval_r) ); return *this; }

        /** Reset value to the current default. */
        void restoreToDefault()
        { this->set( _default.get() ); }

        /** Reset value to a new default. */
        void restoreToDefault( value_type newval_r )
        { setDefault( std::move(newval_r) ); restoreToDefault(); }

        /** Get the current default value. */
        const value_type & getDefault() const
        { return _default.get(); }

        /** Set a new default value. */
        void setDefault( value_type newval_r )
        { _default.set( std::move(newval_r) ); }

        private:
          option_type _default;
      };

}

#endif // ZYPP_CORE_BASE_PRIVATE_CONFIGOPTION_P_H
