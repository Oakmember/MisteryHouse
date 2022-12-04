using Shared;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class PropController : MonoBehaviour
{
    [SerializeField]
    private PropStateType propStateType = PropStateType.None;

    public PropStateType PropStateType
    {
        get => propStateType;
        set => propStateType = value; 
    }

    void Start()
    {
        
    }

    public void Event_SetLayerInteractable()
    {
        //Debug.Log("Interactable");
        SetPropLayer(Consts.interactableLayer);
    }

    public void Event_SetLayerNonInteractable()
    {
        //Debug.Log("NonInteractable");
        SetPropLayer(Consts.nonInteractableLayer);
    }

    private void SetPropLayer(string tagParam)
    {
        this.gameObject.layer = LayerMask.NameToLayer(tagParam);
    }
    
}
