module View exposing (view)

import Html exposing (..)
import Html.Attributes exposing (class,style,placeholder,value,disabled)
import Html.Events exposing (onClick,onInput)
import Selectize exposing (view,viewConfig,autocomplete)

import Model exposing (..)
import Controller exposing (..)

import Dict --for parsing Model.setvars

argView : Dict.Dict String (String,FunArg) -> FunArg -> Html Msg
argView vars arg = 
    let
        s = case (arg.argType) of
            ListArg t2 -> arg.name ++ " (comma separated)"
            _ -> arg.name
        val = Maybe.withDefault "" (Maybe.map (\(x, y) -> x) (Dict.get arg.name vars))
    in
    input [placeholder s, onInput (SetVar arg), value val ] []



funArgsView : Maybe Function -> Dict.Dict String (String,FunArg) -> List (Html Msg )
funArgsView mfun vars = case mfun of
    Nothing ->
        [input [disabled True] []]
    Just fun ->
        List.map (argView vars) fun.args

submitButtonView : Model -> Html Msg
submitButtonView m = 
    let
        btext = [text "Submit"]
        active = button [onClick Submit] btext
        inactive = button [disabled True] btext
        mandatory : FunArg -> Bool
        mandatory arg = not arg.optional
    in case m.selection of
        Nothing -> inactive
        Just fun -> if 
            (List.length <| List.filter mandatory fun.args) == 
            (List.length <| List.filter (\(s,arg)->mandatory arg) <| Dict.values m.setvars )
            then active else inactive

topView : Model -> Html Msg
topView m = div 
    [class "topview"
    ,style "width" "100%"
    ,style "height" "300px"
    ]
    [div 
        [class "leftside"
        ,style "float" "left"
        ,style "width" "auto"
        ]
        [ (map MenuMsg (Selectize.view 
            viewConfig
            m.selection
            m.menu))
        ]
    , div
        [class "rightside"
        ,style "float" "left"
        ,style "width" "auto"
        ]
        [div
            [class "top right"
            ]
            (funArgsView m.selection m.setvars)
        ,div
            [class "bottom right"]
            [submitButtonView m]
        ]
    ]

bottomView : Model -> Html Msg
bottomView m = input
    [style "height" "400px"
    ,style "width" "800px"
    ,value m.resp
    ]
    []

view : Model -> Html Msg
view m =
    div [class "toplevel"
        ,style "width" "100%"
        ,style "display" "block"
        ]
        [ (topView m)
        , (bottomView m )
        ]




andDo : Cmd msg -> ( model, Cmd msg ) -> ( model, Cmd msg )
andDo cmd ( model, cmds ) =
    ( model
    , Cmd.batch [ cmd, cmds ]
    )

entryView : Function -> Bool -> Bool -> Selectize.HtmlDetails Never
entryView fun mouseOn keyOn  =
    let
        cust_attr = if mouseOn then [ style "color" "blue" ] else [style "color" "red"]
        blanket_attr = [ style "list-style-type" "none" ]
    in
    { attributes = cust_attr ++ blanket_attr
    , children =
        [ Html.text
            (fun.name)
        ]
    }

viewConfig : Selectize.ViewConfig Function
viewConfig =
    Selectize.viewConfig
        { container = 
            [ style "position" "relative"
            , style "display" "inline-block"
            ]
        , menu = 
            [ style "width" "100%"
            ]
        , ul = 
            [ style "padding" "0em 1em"
            , style "margin" "0em 0em"
            --https://stackoverflow.com/questions/5219175/width-100-padding#5219611
            , style "box-sizing" "border-box"
            , style "background-color" "#f0f0f0"
            , style "width" "100%"
            , style "border" "1px solid #303030"
            ]
        , entry = entryView
        , divider =
            \title ->
                { attributes =
                    [ class "selectize__divider" ]
                , children =
                    [ Html.text title ]
                }
        , input = styledInput
        }

attrs : Bool -> Bool -> List (Attribute Never)
attrs sthSelected open =
    [style "list-style-type" "none"]

styledInput : Selectize.Input Function
styledInput =
    Selectize.autocomplete <|
        { attrs = attrs
        , toggleButton = Nothing
        , clearButton = Nothing
        , placeholder = "Select a Function"
        }
