import Model exposing (init)
import View exposing (view)
import Controller exposing (update)

import Browser exposing (element)

main = Browser.element
    { init = init
    , subscriptions = \_ -> Sub.none
    , view = view
    , update = update
    }

-- View

