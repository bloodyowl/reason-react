type jsProps;

type reactElement;
type reactRef;
type reactClass;

/* Converters */
[@bs.val] external null: reactElement = "null";
external string: string => reactElement = "%identity";
external array: array(reactElement) => reactElement = "%identity";

[@bs.splice] [@bs.val] [@bs.module "react"]
external createElement:
  (reactClass, ~props: Js.t({..})=?, array(reactElement)) => reactElement =
  "createElement";

[@bs.splice] [@bs.module "react"]
external cloneElement:
  (reactElement, ~props: Js.t({..})=?, array(reactElement)) => reactElement =
  "cloneElement";

[@bs.val] [@bs.module "react"]
external createElementVerbatim: 'a = "createElement";

let createDomElement = (s, ~props, children) => {
  let vararg =
    [|Obj.magic(s), Obj.magic(props)|] |> Js.Array.concat(children);
  /* Use varargs to avoid warnings on duplicate keys in children */
  Obj.magic(createElementVerbatim)##apply(Js.Nullable.null, vararg);
};

type reactComponentBaseClass;

let createClassFactory = [%bs.raw
  {|
function createClass(ReactComponent, spec) {
  var prototype = Object.create(ReactComponent.prototype);
  var statics = { getDerivedStateFromProps: 1, displayName: 1 };
  var lifecycle = {
    componentDidMount: 1,
    componentDidUpdate: 1,
    componentWillUpdate: 1,
    shouldComponentUpdate: 1,
    componentDidCatch: 1,
    componentWillUnmount: 1,
    render: 1
  };
  var specKeys = Object.keys(spec);
  function Component(props, context, updater) {
    ReactComponent.call(this, props, context, updater);
    this.state = this.getInitialState ? this.getInitialState() : null;
    Object.keys(Component.prototype).forEach(function(key) {
      if (typeof this[key] === "function" && !lifecycle[key]) {
        this[key] = this[key].bind(this);
      }
    }, this);
  }
  specKeys.forEach(function(key) {
    if (statics[key]) {
      Component[key] = spec[key];
    } else {
      prototype[key] = spec[key];
    }
  });
  Component.prototype = prototype;
  Component.prototype.constructor = Component;
  Component.prototype.__defined = false;
  return Component;
}

|}
];

[@bs.module "react"]
external reactComponent: reactComponentBaseClass = "Component";

let createClass = spec: reactClass =>
  createClassFactory(. reactComponent, spec);

[@bs.val] external magicNoReactClass: reactClass = "null";

external refToJsObj: reactRef => Js.t({..}) = "%identity";

module WeakMap = {
  type t('value);
  [@bs.new] external make: unit => t('value) = "WeakMap";
  [@bs.send] external set: (t('value), 'key, 'value) => unit = "";
  [@bs.send] external get: (t('value), 'key) => option('value) = "";
};

type jsElementWrapped =
  (
    ~key: Js.nullable(string),
    ~ref: Js.nullable(Js.nullable(reactRef) => unit)
  ) =>
  reactElement;
let jsElementWrappedWeakMap: WeakMap.t(jsElementWrapped) = WeakMap.make();

type subscription = unit => unit;
[@bs.deriving jsConverter]
type didCatchInfo = {componentStack: string};

type update('state, 'action) =
  | NoUpdate
  | Update('state)
  | SideEffects(reducerSelf('state, 'action) => unit)
  | UpdateWithSideEffects('state, reducerSelf('state, 'action) => unit)
and reducerSelf('state, 'action) = {
  handle:
    'payload.
    ('payload, reducerSelf('state, 'action) => unit, 'payload) => unit,

  state: 'state,
  send: 'action => unit,
  onUnmount: subscription => unit,
}
and oldNewSelf('state, 'action) = {
  oldSelf: reducerSelf('state, 'action),
  newSelf: reducerSelf('state, 'action),
}
and reducerComponent('state, 'action) = {
  reactClassInternal: reactClass,
  debugName: string,
  initialState: unit => 'state,
  reducer: ('action, 'state) => update('state, 'action),
  getDerivedStateFromProps: 'state => 'state,
  didMount: reducerSelf('state, 'action) => unit,
  didUpdate: oldNewSelf('state, 'action) => unit,
  didCatch: (reducerSelf('state, 'action), exn, didCatchInfo) => unit,
  willUnmount: reducerSelf('state, 'action) => unit,
  willUpdate: oldNewSelf('state, 'action) => unit,
  shouldUpdate: oldNewSelf('state, 'action) => bool,
  render: reducerSelf('state, 'action) => reactElement,
};

type statelessSelf = {
  handle: 'payload. (('payload, statelessSelf) => unit, 'payload) => unit,
  onUnmount: subscription => unit,
}
and statelessComponent = {
  reactClassInternal: reactClass,
  debugName: string,
  didMount: statelessSelf => unit,
  didUpdate: statelessSelf => unit,
  willUnmount: statelessSelf => unit,
  willUpdate: statelessSelf => unit,
  shouldUpdate: statelessSelf => bool,
  render: statelessSelf => reactElement,
};

type component('a) =
  | Stateless(statelessComponent): component(statelessComponent)
  | Reducer(reducerComponent('state, 'action))
    : component(reducerComponent('state, 'action));

type jsPropsToReason('component, 'props) = 'props => 'component
and jsComponent('component, 'props) = {
  .
  "__defined": bool,
  "props": {. "reasonProps": Js.Nullable.t(component('component))},
  "constructor": jsConstructor('component, 'props),
}
and totalState('state, 'action) = {. "reasonState": 'state}
and jsConstructor('component, 'props) = {
  .
  "jsPropsToReason": option(jsPropsToReason(component('component), 'props)),
}
and jsThis('component, 'props, 'state, 'action) = {
  .
  "__defined": bool,
  "props": {. "reasonProps": Js.Nullable.t(component('component))},
  "state": totalState('state, 'action),
  "setState":
    [@bs.meth] (
      (
        (totalState('state, 'action), 'props) =>
        Js.Null.t(totalState('state, 'action)),
        Js.nullable(unit => unit)
      ) =>
      unit
    ),
  "constructor": jsConstructor('component, 'props),
};

let convertPropsIfTheyreFromJs:
  type spec.
    (
      {. "reasonProps": Js.Nullable.t(component(spec))},
      option(jsPropsToReason(component(spec), 'props)),
      string
    ) =>
    spec =
  (props, jsPropsToReason, debugName) =>
    switch (props##reasonProps->Js.Nullable.toOption, jsPropsToReason) {
    | (Some(props), _) =>
      switch (props) {
      | Stateless(spec) => spec
      | Reducer(spec) => spec
      }
    | (None, Some(toReasonProps)) =>
      switch (toReasonProps()) {
      | Stateless(spec) => spec
      | Reducer(spec) => spec
      }
    | (None, None) =>
      raise(
        Invalid_argument(
          "A JS component called the Reason component "
          ++ debugName
          ++ " which didn't implement the JS->Reason React props conversion.",
        ),
      )
    };

let wrapReasonForJs:
  type spec.
    (
      ~component: component(spec),
      jsPropsToReason(component(spec), 'props)
    ) =>
    reactClass =
  (~component, jsPropsToReason) =>
    switch (component) {
    | Stateless(statelessComponent) =>
      let reactClassInternal = statelessComponent.reactClassInternal;
      reactClassInternal->Obj.magic##jsPropsToReason #= Some(jsPropsToReason);
      reactClassInternal;
    | Reducer(reducerComponent) =>
      let reactClassInternal = reducerComponent.reactClassInternal;
      reactClassInternal->Obj.magic##jsPropsToReason #= Some(jsPropsToReason);
      reactClassInternal;
    };

module Component = {
  let anyToUnit = _ => ();
  let anyToTrue = _ => true;
  let defaultRender = _ => string("RenderNotImplemented");
  module Reducer = {
    let defaultReducer = (_, _) => NoUpdate;
    let defaultGetDerivedStateFromProps = state => state;
    let defaultDidCatch = (_self, _exn, _jsInfo) => ();

    module JsSpec = {
      type jsStatefulComponent('state, 'action, 'payload, 'jsProps, 'jsState) = {
        .
        "displayName": string,
        "subscriptions": Js.Null.t(array(subscription)),
        "self": unit => reducerSelf('state, 'action),
        "sendMethod": 'action => unit,
        "handleMethod":
          (('payload, reducerSelf('state, 'action)) => unit, 'payload) => unit,
        "onUnmountMethod": subscription => unit,
        "getInitialState": unit => totalState('state, 'action),
        "componentDidMount": Js.Undefined.t(unit => unit),
        "componentDidUpdate": Js.Undefined.t(('jsProps, 'jsState) => unit),
        "componentDidCatch":
          Js.Undefined.t((exn, {. "componentStack": string}) => unit),
        "getDerivedStateFromProps":
          Js.Undefined.t(('jsProps, 'jsState) => 'jsState),
        "componentWillUnmount": unit => unit,
        "componentWillUpdate": Js.Undefined.t(('jsProps, 'jsState) => unit),
        "shouldComponentUpdate": Js.Undefined.t(('jsProps, 'jsState) => bool),
        "render": unit => reactElement,
      };
      [@bs.val]
      external this:
        jsStatefulComponent('state, 'action, 'payload, 'jsProps, 'jsState) =
        "this";
      [@bs.val]
      external staticThis:
        jsConstructor(reducerComponent('state, 'action), 'jsProps) =
        "this";
      [@bs.set]
      external addSubscription:
        (
          jsStatefulComponent('state, 'action, 'payload, 'jsProps, 'jsState),
          Js.Null.t(array(subscription))
        ) =>
        unit =
        "subscriptions";
      [@bs.val]
      external thisJs:
        jsThis(reducerComponent('state, 'action), 'props, 'state, 'action) =
        "this";
      [@bs.set]
      external setDefined:
        (
          jsThis(reducerComponent('state, 'action), 'props, 'state, 'action),
          bool
        ) =>
        unit =
        "__defined";
      let make = (spec: reducerComponent('state, 'action)): reactClass => {
        let reactClass = ref(None);
        let handleMethod = (callback, callbackPayload): unit =>
          callback(
            callbackPayload,
            {
              let self = this##self;
              self();
            },
          );
        let sendMethod = (action: 'action) => {
          let component: reducerComponent('state, 'action) =
            convertPropsIfTheyreFromJs(
              thisJs##props,
              thisJs##constructor##jsPropsToReason,
              spec.debugName,
            );
          if (component.reducer !== defaultReducer) {
            let sideEffect = ref(ignore);
            thisJs##setState(
              (currentTotalState, _) => {
                let currentReasonState = currentTotalState##reasonState;
                switch (component.reducer(action, currentReasonState)) {
                | NoUpdate => Js.Null.empty
                | Update(nextState) =>
                  {"reasonState": nextState}->Js.Null.return
                | SideEffects(performSideEffects) =>
                  sideEffect := performSideEffects;
                  Js.Null.empty;
                | UpdateWithSideEffects(nextState, performSideEffects) =>
                  sideEffect := performSideEffects;
                  {"reasonState": nextState}->Js.Null.return;
                };
              },
              Js.Nullable.return(
                {
                  let handleMethod = this##handleMethod;
                  handleMethod(((), self) => sideEffect^(self));
                },
              ),
            );
          };
        };
        let onUnmountMethod = subscription =>
          switch (this##subscriptions->Js.Null.toOption) {
          | Some(subscriptions) =>
            Js.Array.push(subscription, subscriptions)->ignore
          | None => addSubscription(this, Js.Null.return([|subscription|]))
          };
        let self = state => {
          handle: this##handleMethod->Obj.magic,
          onUnmount: this##onUnmountMethod,
          state,
          send: this##sendMethod->Obj.magic,
        };

        let getInitialState = () => {
          let component: reducerComponent('state, 'action) =
            convertPropsIfTheyreFromJs(
              thisJs##props,
              thisJs##constructor##jsPropsToReason,
              spec.debugName,
            );

          if (!thisJs##__defined) {
            setDefined(thisJs, true);
            let componentDidMount =
              component.didMount === anyToUnit ?
                Js.Undefined.empty :
                Js.Undefined.return(() => {
                  let component: reducerComponent('state, 'action) =
                    convertPropsIfTheyreFromJs(
                      thisJs##props,
                      thisJs##constructor##jsPropsToReason,
                      spec.debugName,
                    );
                  component.didMount(
                    {
                      let self = this##self;
                      self();
                    },
                  );
                });
            let componentDidUpdate =
              component.didUpdate === anyToUnit ?
                Js.Undefined.empty :
                Js.Undefined.return((_prevProps, prevState) => {
                  let component: reducerComponent('state, 'action) =
                    convertPropsIfTheyreFromJs(
                      thisJs##props,
                      thisJs##constructor##jsPropsToReason,
                      spec.debugName,
                    );
                  component.didUpdate({
                    newSelf: {
                      let self = this##self;
                      self(thisJs##state##reasonState);
                    },
                    oldSelf: {
                      let self = this##self;
                      self(prevState##reasonState);
                    },
                  });
                });
            let componentWillUpdate =
              component.willUpdate === anyToUnit ?
                Js.Undefined.empty :
                Js.Undefined.return((_nextProps, nextState) => {
                  let component: reducerComponent('state, 'action) =
                    convertPropsIfTheyreFromJs(
                      thisJs##props,
                      thisJs##constructor##jsPropsToReason,
                      spec.debugName,
                    );
                  component.willUpdate({
                    newSelf: {
                      let self = this##self;
                      self(nextState##reasonState);
                    },
                    oldSelf: {
                      let self = this##self;
                      self(thisJs##state##reasonState);
                    },
                  });
                });
            let componentDidCatch =
              component.didCatch === defaultDidCatch ?
                Js.Undefined.empty :
                Js.Undefined.return((exn, jsInfo) => {
                  let component: reducerComponent('state, 'action) =
                    convertPropsIfTheyreFromJs(
                      thisJs##props,
                      thisJs##constructor##jsPropsToReason,
                      spec.debugName,
                    );
                  component.didCatch(
                    {
                      let self = this##self;
                      self(thisJs##state##reasonState);
                    },
                    exn,
                    jsInfo->didCatchInfoFromJs,
                  );
                });
            let getDerivedStateFromProps =
              component.getDerivedStateFromProps
              === defaultGetDerivedStateFromProps ?
                None :
                Some(
                  (props, state) => {
                    let component: reducerComponent('state, 'action) =
                      convertPropsIfTheyreFromJs(
                        props,
                        staticThis##jsPropsToReason,
                        spec.debugName,
                      );
                    let reasonState =
                      component.getDerivedStateFromProps(state##reasonState);
                    {"reasonState": reasonState};
                  },
                );
            let componentWillUnmount = () => {
              let component: reducerComponent('state, 'action) =
                convertPropsIfTheyreFromJs(
                  thisJs##props,
                  thisJs##constructor##jsPropsToReason,
                  spec.debugName,
                );
              if (component.willUnmount !== anyToUnit) {
                component.willUnmount(
                  {
                    let self = this##self;
                    self();
                  },
                );
              };
              switch (this##subscriptions->Js.Null.toOption) {
              | Some(subscriptions) =>
                Js.Array.forEach(unsubscribe => unsubscribe(), subscriptions)
              | None => ()
              };
            };
            let shouldComponentUpdate =
              component.shouldUpdate === anyToTrue ?
                Js.Undefined.empty :
                Js.Undefined.return((_nextProps, nextState) => {
                  let component: reducerComponent('state, 'action) =
                    convertPropsIfTheyreFromJs(
                      thisJs##props,
                      thisJs##constructor##jsPropsToReason,
                      spec.debugName,
                    );
                  component.shouldUpdate({
                    newSelf: {
                      let self = this##self;
                      self(nextState##reasonState);
                    },
                    oldSelf: {
                      let self = this##self;
                      self(thisJs##state##reasonState);
                    },
                  });
                });
            let render = () => {
              let component: reducerComponent('state, 'action) =
                convertPropsIfTheyreFromJs(
                  thisJs##props,
                  thisJs##constructor##jsPropsToReason,
                  spec.debugName,
                );
              component.render(
                {
                  let self = this##self;
                  self(thisJs##state##reasonState);
                },
              );
            };

            switch (reactClass^) {
            | Some(reactClass) =>
              let extensibleReactClass = Obj.magic(reactClass);
              extensibleReactClass##prototype##componentDidMount
              #= componentDidMount;
              extensibleReactClass##prototype##componentDidUpdate
              #= componentDidUpdate;
              extensibleReactClass##prototype##componentDidCatch
              #= componentDidCatch;
              extensibleReactClass##getDerivedStateFromProps
              #= (
                   switch (getDerivedStateFromProps) {
                   | Some(getDerivedStateFromProps) =>
                     Obj.magic(getDerivedStateFromProps)##bind(
                       extensibleReactClass,
                     )
                     ->Js.Undefined.return
                   | None => Js.Undefined.empty
                   }
                 );
              extensibleReactClass##prototype##componentWillUnmount
              #= componentWillUnmount;
              extensibleReactClass##prototype##componentWillUpdate
              #= componentWillUpdate;
              extensibleReactClass##prototype##shouldComponentUpdate
              #= shouldComponentUpdate;
              extensibleReactClass##prototype##render #= render;
            | None => ()
            };
          };
          let initialReasonState = component.initialState();
          {"reasonState": initialReasonState};
        };
        reactClass :=
          Some(
            createClass({
              "displayName": spec.debugName,
              "subscriptions": Js.Null.empty,
              "self": self,
              "handleMethod": handleMethod,
              "sendMethod": sendMethod,
              "onUnmountMethod": onUnmountMethod,
              /* lifecycle */
              "getInitialState": getInitialState,
              "componentDidMount": Js.Undefined.empty,
              "componentDidUpdate": Js.Undefined.empty,
              "componentDidCatch": Js.Undefined.empty,
              "getDerivedStateFromProps": Js.Undefined.empty,
              "componentWillUnmount": Js.Undefined.empty,
              "componentWillUpdate": Js.Undefined.empty,
              "shouldComponentUpdate": Js.Undefined.empty,
              "render": Js.Undefined.empty,
            }),
          );
        Obj.magic(reactClass^);
      };
    };

    external defaultInitialState: unit => 'state = "%identity";
    let make = debugName => {
      let spec = {
        reactClassInternal: magicNoReactClass,
        debugName,
        initialState: defaultInitialState,
        reducer: defaultReducer,
        getDerivedStateFromProps: defaultGetDerivedStateFromProps,
        didMount: anyToUnit,
        didUpdate: anyToUnit,
        didCatch: defaultDidCatch,
        willUnmount: anyToUnit,
        willUpdate: anyToUnit,
        shouldUpdate: anyToTrue,
        render: defaultRender,
      };
      {...spec, reactClassInternal: JsSpec.make(spec)};
    };
  };
  module Stateless = {
    module JsSpec = {
      type jsStatelessSpec('payload) = {
        .
        "displayName": string,
        "subscriptions": Js.Null.t(array(subscription)),
        "self": unit => statelessSelf,
        "handleMethod": (('payload, statelessSelf) => unit, 'payload) => unit,
        "onUnmountMethod": subscription => unit,
        "getInitialState": unit => Js.Null.t(unit),
        "componentDidMount": Js.Undefined.t(unit => unit),
        "componentDidUpdate": Js.Undefined.t(unit => unit),
        "componentWillUnmount": unit => unit,
        "componentWillUpdate": Js.Undefined.t(unit => unit),
        "shouldComponentUpdate": Js.Undefined.t(unit => bool),
        "render": unit => reactElement,
      };
      [@bs.val] external this: jsStatelessSpec('payload) = "this";
      [@bs.set]
      external addSubscription:
        (jsStatelessSpec('payload), Js.Null.t(array(subscription))) => unit =
        "subscriptions";
      [@bs.val]
      external thisJs: jsComponent(statelessComponent, 'props) = "this";
      [@bs.set]
      external setDefined:
        (jsComponent(statelessComponent, 'props), bool) => unit =
        "__defined";
      let make = (spec: statelessComponent): reactClass => {
        let reactClass = ref(None);
        let handleMethod = (callback, callbackPayload): unit =>
          callback(
            callbackPayload,
            {
              let self = this##self;
              self();
            },
          );
        let onUnmountMethod = subscription =>
          switch (this##subscriptions->Js.Null.toOption) {
          | Some(subscriptions) =>
            Js.Array.push(subscription, subscriptions)->ignore
          | None => addSubscription(this, Js.Null.return([|subscription|]))
          };

        let getInitialState = () => {
          let component: statelessComponent =
            convertPropsIfTheyreFromJs(
              thisJs##props,
              thisJs##constructor##jsPropsToReason,
              spec.debugName,
            );
          if (!thisJs##__defined) {
            setDefined(thisJs, true);
            let componentDidMount =
              component.didMount === anyToUnit ?
                Js.Undefined.empty :
                Js.Undefined.return(() => {
                  let component: statelessComponent =
                    convertPropsIfTheyreFromJs(
                      thisJs##props,
                      thisJs##constructor##jsPropsToReason,
                      spec.debugName,
                    );
                  component.didMount(
                    {
                      let self = this##self;
                      self();
                    },
                  );
                });
            let componentDidUpdate =
              component.didUpdate === anyToUnit ?
                Js.Undefined.empty :
                Js.Undefined.return(() => {
                  let component: statelessComponent =
                    convertPropsIfTheyreFromJs(
                      thisJs##props,
                      thisJs##constructor##jsPropsToReason,
                      spec.debugName,
                    );
                  component.didUpdate(
                    {
                      let self = this##self;
                      self();
                    },
                  );
                });
            let componentWillUpdate =
              component.willUpdate === anyToUnit ?
                Js.Undefined.empty :
                Js.Undefined.return(() => {
                  let component: statelessComponent =
                    convertPropsIfTheyreFromJs(
                      thisJs##props,
                      thisJs##constructor##jsPropsToReason,
                      spec.debugName,
                    );
                  component.willUpdate(
                    {
                      let self = this##self;
                      self();
                    },
                  );
                });
            let componentWillUnmount = () => {
              let component: statelessComponent =
                convertPropsIfTheyreFromJs(
                  thisJs##props,
                  thisJs##constructor##jsPropsToReason,
                  spec.debugName,
                );
              if (component.willUnmount !== anyToUnit) {
                component.willUnmount(
                  {
                    let self = this##self;
                    self();
                  },
                );
              };
              switch (this##subscriptions->Js.Null.toOption) {
              | Some(subscriptions) =>
                Js.Array.forEach(unsubscribe => unsubscribe(), subscriptions)
              | None => ()
              };
            };
            let shouldComponentUpdate =
              component.shouldUpdate === anyToTrue ?
                Js.Undefined.empty :
                Js.Undefined.return(() => {
                  let component: statelessComponent =
                    convertPropsIfTheyreFromJs(
                      thisJs##props,
                      thisJs##constructor##jsPropsToReason,
                      spec.debugName,
                    );
                  component.shouldUpdate(
                    {
                      let self = this##self;
                      self();
                    },
                  );
                });
            let render = () => {
              let component: statelessComponent =
                convertPropsIfTheyreFromJs(
                  thisJs##props,
                  thisJs##constructor##jsPropsToReason,
                  spec.debugName,
                );
              component.render(
                {
                  let self = this##self;
                  self();
                },
              );
            };

            switch (reactClass^) {
            | Some(reactClass) =>
              let extensibleReactClass = Obj.magic(reactClass);
              extensibleReactClass##prototype##componentDidMount
              #= componentDidMount;
              extensibleReactClass##prototype##componentDidUpdate
              #= componentDidUpdate;
              extensibleReactClass##prototype##componentWillUnmount
              #= componentWillUnmount;
              extensibleReactClass##prototype##componentWillUpdate
              #= componentWillUpdate;
              extensibleReactClass##prototype##shouldComponentUpdate
              #= shouldComponentUpdate;
              extensibleReactClass##prototype##render #= render;
            | None => ()
            };
          };
        };
        let self = () => {
          handle: this##handleMethod->Obj.magic,
          onUnmount: this##onUnmountMethod,
        };
        reactClass :=
          Some({
            "displayName": spec.debugName,
            "subscriptions": Js.Null.empty,
            "self": self,
            "handleMethod": handleMethod,
            "onUnmountMethod": onUnmountMethod,
            "getInitialState": getInitialState,
            /* lifecycle */
            "componentDidMount": Js.Undefined.empty,
            "componentDidUpdate": Js.Undefined.empty,
            "componentWillUnmount": Js.Undefined.empty,
            "componentWillUpdate": Js.Undefined.empty,
            "shouldComponentUpdate": Js.Undefined.empty,
            "render": Js.Undefined.empty,
          });
        Obj.magic(reactClass^);
      };
    };
    let make = debugName => {
      let spec = {
        reactClassInternal: magicNoReactClass,
        debugName,
        didMount: anyToUnit,
        didUpdate: anyToUnit,
        willUnmount: anyToUnit,
        willUpdate: anyToUnit,
        shouldUpdate: anyToTrue,
        render: defaultRender,
      };
      {
        ...spec,
        reactClassInternal: createClass(JsSpec.make(spec)->Obj.magic),
      };
    };
  };
};

let statelessComponent = Component.Stateless.make;
let reducerComponent = Component.Reducer.make;

let element:
  type spec.
    (
      ~key: string=?,
      ~ref: Js.nullable(reactRef) => unit=?,
      component(spec)
    ) =>
    reactElement =
  (
    ~key=Obj.magic(Js.Nullable.undefined),
    ~ref=Obj.magic(Js.Nullable.undefined),
    element,
  ) =>
    switch (WeakMap.get(jsElementWrappedWeakMap, element)) {
    | Some(jsElementWrapped) =>
      jsElementWrapped(
        ~key=Js.Nullable.return(key),
        ~ref=Js.Nullable.return(ref),
      )
    | None =>
      switch (element) {
      | Stateless(statelessComponent) =>
        createElement(
          statelessComponent.reactClassInternal,
          ~props={"key": key, "ref": ref, "reasonProps": element},
          [||],
        )
      | Reducer(reducerComponent) =>
        createElement(
          reducerComponent.reactClassInternal,
          ~props={"key": key, "ref": ref, "reasonProps": element},
          [||],
        )
      }
    };

module WrapProps = {
  /* We wrap the props for reason->reason components, as a marker that "these props were passed from another
     reason component" */
  let wrapProps =
      (
        ~reactClass,
        ~props,
        children,
        ~key: Js.nullable(string),
        ~ref: Js.nullable(Js.nullable(reactRef) => unit),
      ) => {
    let props =
      Js.Obj.assign(
        Js.Obj.assign(Js.Obj.empty(), Obj.magic(props)),
        {"ref": ref, "key": key},
      );
    let varargs =
      [|Obj.magic(reactClass), Obj.magic(props)|]
      |> Js.Array.concat(Obj.magic(children));
    /* Use varargs under the hood */
    Obj.magic(createElementVerbatim)##apply(Js.Nullable.null, varargs);
  };
  let dummyInteropComponent = Component.Stateless.make("interop");
  let wrapJsForReason = (~reactClass, ~props, children): statelessComponent => {
    let jsElementWrapped = wrapProps(~reactClass, ~props, children);
    WeakMap.set(
      jsElementWrappedWeakMap,
      dummyInteropComponent,
      jsElementWrapped,
    );
    dummyInteropComponent;
  };
};

let wrapJsForReason = WrapProps.wrapJsForReason;

[@bs.module "react"] external fragment: 'a = "Fragment";

module Router = ReasonReactRouter;
