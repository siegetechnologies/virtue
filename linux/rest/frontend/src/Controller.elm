module Controller exposing (Msg(..),update)

import Selectize exposing (Msg,update)

import Model exposing (..)

import Dict
import Json.Encode exposing (Value,object,string,encode)
import Json.Decode exposing (value) --for Http post decoder
import Url exposing (Url,Protocol(..),toString)
import Http exposing (send,post,jsonBody)
import Result exposing (Result(..))

ccserver : Url
ccserver =
    { protocol = Http
    , host = "ec2-35-172-214-46.compute-1.amazonaws.com"
    , port_ = Just 4443
    , path = ""
    , query = Nothing
    , fragment = Nothing
    }

-- expect String to be something like "user login"
-- convert to {ccserver}/usr/login
endpoint : String -> String
endpoint cmd  = Url.toString { ccserver | path = "/" ++ (String.join "/" (String.split " " cmd)) }


-- Updating
type Msg
    = MenuMsg (Selectize.Msg Function)
    | SelectFun (Maybe Function)
    | SetVar FunArg String -- arg, value
    | Submit
    | ServerResp (Result Http.Error Value)

-- could probably conditionally update stuff.
-- eg only allow 0-9 in IntArgs
updateVar : FunArg -> String -> Model -> Model
updateVar arg v mod = { mod | setvars = Dict.insert arg.name (v,arg) mod.setvars }

createJson : Model -> Value
createJson mod = 
    let
        d = mod.setvars
        foldfun : String -> (String,FunArg) -> List (String, Value) -> List (String, Value)
        foldfun k (v,arg) valList = 
            case argEncode arg.argType v of
                Nothing -> valList
                Just val -> (k, val)::valList
    in
        object (Dict.foldl foldfun [] d)

sendHttp : String -> Value -> Cmd Msg
sendHttp endpt val =
    let
        body = jsonBody val
        respDecoder = value --just get me the value
        req = Http.post endpt body respDecoder
    in
        Http.send ServerResp req


update : Msg -> Model -> (Model, Cmd Msg)
update msg mod = case msg of
    MenuMsg (selmsg) -> 
        let
            ( newMenu, menuCmd, maybeMsg ) = 
                Selectize.update
                    SelectFun
                    mod.selection
                    mod.menu
                    selmsg
            newMod = { mod | menu = newMenu }
        in case maybeMsg of 
            Just nextMsg -> update nextMsg newMod
            Nothing -> (newMod,Cmd.none)
    SelectFun (mfun) -> case mfun of
        Nothing -> ({ mod | selection = mfun, setvars = Dict.empty },Cmd.none)
        Just fun ->
            let
                dempty = Dict.fromList <| List.map (\arg -> (arg.name,("",arg))) fun.args
                d2 = Dict.intersect mod.setvars dempty
            in ({ mod | selection = mfun, setvars = d2 },Cmd.none)
    Submit ->
        case mod.selection of
            Nothing -> (mod,Cmd.none)
            Just fun -> (mod,sendHttp (endpoint fun.name) (createJson mod))

    SetVar k v -> (updateVar k v mod,Cmd.none)
    ServerResp res -> case res of
        Ok val -> ({mod | resp = encode 4 val},Cmd.none)
        Err _ -> (mod,Cmd.none) -- todo deal with errors
        -- https://github.com/elm/http/blob/master/src/Http.elm#L115
