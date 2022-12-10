using Shared;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class HandData : MonoBehaviour
{
    [SerializeField]
    private HandType handType = HandType.None;

    [SerializeField]
    private Transform root = null;
    [SerializeField]
    private Animator animator = null;
    [SerializeField]
    private Transform[] fingerBones = null;

    public Animator Animator => animator;
    public Transform Root => root;
    public Transform[] FingerBones => fingerBones;
    public HandType HandType => handType;

    void Start()
    {
        
    }

   
}
